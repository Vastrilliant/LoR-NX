// Shader library download for the setup screen: fetch nxnative-shaders.zip from the repository's latest release, unpack its msl/ folder (the README beside it is dropped) and move
// that folder to Documents/msl, replacing the one there. main.m (msl_locate) then decides whether the set is the right one for this build.
// The release's zip is written with `zip -0` (stored, .github/workflows/build.yml), but entries that are deflated are read as well. Zip64 archives are not (the asset stays under 2 GiB).
#import "ShaderDownload.h"
#import <compression.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

static NSString *const kShadersURL = @"https://github.com/Vastrilliant/LoR-NX/releases/latest/download/nxnative-shaders.zip";
static NSString *const kErrorDomain = @"DmShaderDownload";
static volatile int g_cancel;   // set by dm_shader_download_cancel, read by the unpacking loops

static NSError *shader_error(NSString *message) { return [NSError errorWithDomain:kErrorDomain code:1 userInfo:@{NSLocalizedDescriptionKey: message}]; }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static int write_all(int fd, const uint8_t *p, size_t n) {
    while (n) { ssize_t w = write(fd, p, n); if (w < 0 && errno == EINTR) continue; if (w <= 0) return 0; p += w; n -= (size_t)w; }
    return 1;
}
static int read_at(int fd, void *buf, size_t n, off_t at) {
    uint8_t *p = buf;
    while (n) { ssize_t r = pread(fd, p, n, at); if (r < 0 && errno == EINTR) continue; if (r <= 0) return 0; p += r; at += r; n -= (size_t)r; }
    return 1;
}

@interface DmZipEntry : NSObject { @public NSString *rel; BOOL dir; uint16_t method; uint64_t csize, usize, local; } @end
@implementation DmZipEntry @end

// The entries under msl/ with their path relative to msl/. Anything else (README.txt, __MACOSX) is left out; a path that could leave the folder is an error
static NSArray<DmZipEntry *> *zip_msl_entries(int fd, off_t size, NSError **err) {
    size_t tail = (size_t)MIN((off_t)(65535 + 22), size); uint8_t *buf = malloc(tail);
    if (!buf || !read_at(fd, buf, tail, size - (off_t)tail)) { free(buf); *err = shader_error(@"The downloaded file could not be read."); return nil; }
    ssize_t e = -1;
    for (ssize_t i = (ssize_t)tail - 22; i >= 0; i--) if (rd32(buf + i) == 0x06054b50) { e = i; break; }
    if (e < 0) { free(buf); *err = shader_error(@"The downloaded file is not a zip archive."); return nil; }
    uint16_t count = rd16(buf + e + 10); uint32_t cd_size = rd32(buf + e + 12), cd_off = rd32(buf + e + 16); free(buf);
    if (count == 0xFFFF || cd_size == 0xFFFFFFFFu || cd_off == 0xFFFFFFFFu || (off_t)cd_off + (off_t)cd_size > size) { *err = shader_error(@"The downloaded zip archive is not supported."); return nil; }
    uint8_t *cd = malloc(cd_size ?: 1);
    if (!cd || !read_at(fd, cd, cd_size, cd_off)) { free(cd); *err = shader_error(@"The downloaded file could not be read."); return nil; }
    NSMutableArray<DmZipEntry *> *out = [NSMutableArray array]; size_t at = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (at + 46 > cd_size || rd32(cd + at) != 0x02014b50) { free(cd); *err = shader_error(@"The downloaded zip archive is damaged."); return nil; }
        uint16_t flags = rd16(cd + at + 8), method = rd16(cd + at + 10), nlen = rd16(cd + at + 28), xlen = rd16(cd + at + 30), clen = rd16(cd + at + 32);
        uint32_t csize = rd32(cd + at + 20), usize = rd32(cd + at + 24), local = rd32(cd + at + 42);
        if (at + 46 + nlen + xlen + clen > cd_size) { free(cd); *err = shader_error(@"The downloaded zip archive is damaged."); return nil; }
        NSString *name = memchr(cd + at + 46, 0, nlen) ? nil : [[NSString alloc] initWithBytes:cd + at + 46 length:nlen encoding:NSUTF8StringEncoding];
        at += 46 + nlen + xlen + clen;
        if (!name) { free(cd); *err = shader_error(@"The downloaded zip archive has an unreadable file name."); return nil; }
        if (![name hasPrefix:@"msl/"]) continue;
        NSString *rel = [name substringFromIndex:4]; BOOL dir = [rel hasSuffix:@"/"];
        if (dir) rel = [rel substringToIndex:rel.length - 1];
        if (!rel.length) continue;   // msl/ itself
        for (NSString *part in [rel componentsSeparatedByString:@"/"])
            if (!part.length || [part isEqualToString:@".."] || [part isEqualToString:@"."] || [part containsString:@"\\"]) { free(cd); *err = shader_error(@"The downloaded zip archive has an unsafe file name."); return nil; }
        if (flags & 1) { free(cd); *err = shader_error(@"The downloaded zip archive is encrypted."); return nil; }
        if (csize == 0xFFFFFFFFu || usize == 0xFFFFFFFFu || local == 0xFFFFFFFFu) { free(cd); *err = shader_error(@"The downloaded zip archive is not supported."); return nil; }
        if (method != 0 && method != 8) { free(cd); *err = shader_error(@"The downloaded zip archive uses an unsupported compression."); return nil; }
        DmZipEntry *z = [DmZipEntry new];
        z->rel = rel; z->dir = dir; z->method = method; z->csize = csize; z->usize = usize; z->local = local;
        [out addObject:z];
    }
    free(cd); return out;
}

// Writes one entry's data to dst. Stored: a copy. Deflated: raw deflate through Apple's compression library (COMPRESSION_ZLIB is the raw format a zip holds)
static NSError *zip_extract(int fd, DmZipEntry *z, NSString *dst, uint64_t *done, uint64_t total, void (^prog)(double)) {
    uint8_t lh[30];
    if (!read_at(fd, lh, sizeof lh, (off_t)z->local) || rd32(lh) != 0x04034b50) return shader_error(@"The downloaded zip archive is damaged.");
    off_t data = (off_t)z->local + 30 + rd16(lh + 26) + rd16(lh + 28);
    int out = open(dst.fileSystemRepresentation, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) return [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Could not create %@ (%s).", dst.lastPathComponent, strerror(errno)]}];
    enum { CHUNK = 1 << 20 };
    uint8_t *in = malloc(CHUNK), *buf = malloc(CHUNK); NSError *err = nil; uint64_t written = 0, remaining = z->csize;
    compression_stream s; int have_stream = 0;
    if (!in || !buf) err = shader_error(@"Out of memory.");
    if (!err && z->method == 8) {
        if (compression_stream_init(&s, COMPRESSION_STREAM_DECODE, COMPRESSION_ZLIB) != COMPRESSION_STATUS_OK) err = shader_error(@"Could not start unpacking.");
        else { have_stream = 1; s.src_ptr = in; s.src_size = 0; s.dst_ptr = buf; s.dst_size = CHUNK; }
    }
    if (!err && z->method == 0) {
        if (z->csize != z->usize) err = shader_error(@"The downloaded zip archive is damaged.");
        for (off_t at = data; !err && remaining;) {
            size_t n = (size_t)MIN((uint64_t)CHUNK, remaining);
            if (g_cancel) err = [NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorCancelled userInfo:nil];
            else if (!read_at(fd, in, n, at)) err = shader_error(@"The downloaded file could not be read.");
            else if (!write_all(out, in, n)) err = shader_error(@"Could not write the shader libraries (is the device full?).");
            else { at += (off_t)n; remaining -= n; written += n; *done += n; prog(total ? (double)*done / (double)total : -1); }
        }
    } else if (!err) {
        compression_status st = COMPRESSION_STATUS_OK; off_t at = data;
        do {
            if (g_cancel) { err = [NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorCancelled userInfo:nil]; break; }
            if (s.src_size == 0 && remaining) {
                size_t n = (size_t)MIN((uint64_t)CHUNK, remaining);
                if (!read_at(fd, in, n, at)) { err = shader_error(@"The downloaded file could not be read."); break; }
                at += (off_t)n; remaining -= n; s.src_ptr = in; s.src_size = n;
            }
            st = compression_stream_process(&s, remaining ? 0 : COMPRESSION_STREAM_FINALIZE);
            if (st == COMPRESSION_STATUS_ERROR) { err = shader_error(@"The downloaded zip archive is damaged."); break; }
            size_t produced = CHUNK - s.dst_size;
            if (!produced && st == COMPRESSION_STATUS_OK && !s.src_size && !remaining) { err = shader_error(@"The downloaded zip archive is damaged."); break; }   // the data ended inside the stream
            if (produced) {
                if (!write_all(out, buf, produced)) { err = shader_error(@"Could not write the shader libraries (is the device full?)."); break; }
                written += produced; *done += produced; prog(total ? (double)*done / (double)total : -1);
                s.dst_ptr = buf; s.dst_size = CHUNK;
            }
        } while (st == COMPRESSION_STATUS_OK);
        if (!err && st != COMPRESSION_STATUS_END) err = shader_error(@"The downloaded zip archive is damaged.");
    }
    if (have_stream) compression_stream_destroy(&s);
    free(in); free(buf); close(out);
    if (!err && written != z->usize) err = shader_error(@"The downloaded zip archive is damaged.");
    return err;
}

// zip -> Documents/msl: unpacked beside it first (Documents/msl.download), then swapped in, so a failed or cancelled download leaves the msl folder that was there
static NSError *unpack_msl(NSString *zip, NSString *doc, void (^progress)(double)) {
    NSFileManager *fm = NSFileManager.defaultManager;
    int fd = open(zip.fileSystemRepresentation, O_RDONLY); if (fd < 0) return shader_error(@"The downloaded file could not be opened.");
    struct stat st; if (fstat(fd, &st) || st.st_size < 22) { close(fd); return shader_error(@"The downloaded file is not a zip archive."); }
    NSError *err = nil;
    NSArray<DmZipEntry *> *entries = zip_msl_entries(fd, st.st_size, &err);
    if (!entries) { close(fd); return err; }
    if (!entries.count) { close(fd); return shader_error(@"The downloaded archive has no msl folder."); }
    uint64_t total = 0; for (DmZipEntry *z in entries) if (!z->dir) total += z->usize;
    NSString *stage = [doc stringByAppendingPathComponent:@"msl.download"], *msl = [doc stringByAppendingPathComponent:@"msl"];
    [fm removeItemAtPath:stage error:nil];
    if (![fm createDirectoryAtPath:stage withIntermediateDirectories:YES attributes:nil error:&err]) { close(fd); return err; }
    uint64_t done = 0;
    for (DmZipEntry *z in entries) {
        NSString *dst = [stage stringByAppendingPathComponent:z->rel];
        if (z->dir) { if (![fm createDirectoryAtPath:dst withIntermediateDirectories:YES attributes:nil error:&err]) break; continue; }
        if (![fm createDirectoryAtPath:dst.stringByDeletingLastPathComponent withIntermediateDirectories:YES attributes:nil error:&err]) break;
        err = zip_extract(fd, z, dst, &done, total, progress); if (err) break;
    }
    close(fd);
    if (!err && ![fm fileExistsAtPath:[stage stringByAppendingPathComponent:@"msl.id"]]) err = shader_error(@"The downloaded shader libraries have no msl.id.");
    if (err) { [fm removeItemAtPath:stage error:nil]; return err; }
    NSString *old = nil;
    if ([fm fileExistsAtPath:msl]) {
        old = [doc stringByAppendingPathComponent:[@"msl.old-" stringByAppendingString:NSUUID.UUID.UUIDString]];
        if (![fm moveItemAtPath:msl toPath:old error:&err]) { [fm removeItemAtPath:stage error:nil]; return err; }
    }
    if (![fm moveItemAtPath:stage toPath:msl error:&err]) { if (old) [fm moveItemAtPath:old toPath:msl error:nil]; [fm removeItemAtPath:stage error:nil]; return err; }
    if (old) [fm removeItemAtPath:old error:nil];
    [[NSURL fileURLWithPath:msl] setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:nil];   // like the other game files (main.m)
    return nil;
}

@interface DmShaderDownloader : NSObject <NSURLSessionDownloadDelegate>
@property (nonatomic, copy) NSString *doc;
@property (nonatomic, copy) DmShaderProgress progress;
@property (nonatomic, copy) void (^done)(NSError *);
@property (nonatomic, strong) NSURLSession *session;
@property (nonatomic, strong) NSURLSessionDownloadTask *task;
@property (nonatomic) BOOL finished;
@end
static DmShaderDownloader *g_downloader;

@implementation DmShaderDownloader
- (void)finish:(NSError *)error {   // once, on the main queue
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.finished) return; self.finished = YES;
        [self.session finishTasksAndInvalidate];
        if (g_downloader == self) g_downloader = nil;
        fprintf(stderr, error ? "[RED] boot: shader download failed: %s\n" : "boot: shader libraries downloaded to Documents/msl\n", error.localizedDescription.UTF8String);
        self.done(error);
    });
}
- (void)URLSession:(NSURLSession *)s downloadTask:(NSURLSessionDownloadTask *)t didWriteData:(int64_t)n totalBytesWritten:(int64_t)w totalBytesExpectedToWrite:(int64_t)e {
    (void)s; (void)t; (void)n;
    double f = e > 0 ? (double)w / (double)e : -1;
    dispatch_async(dispatch_get_main_queue(), ^{ if (!self.finished) self.progress(@"Downloading", f); });
}
- (void)URLSession:(NSURLSession *)s downloadTask:(NSURLSessionDownloadTask *)t didFinishDownloadingToURL:(NSURL *)location {
    (void)s;
    NSInteger code = [t.response isKindOfClass:NSHTTPURLResponse.class] ? ((NSHTTPURLResponse *)t.response).statusCode : 0;
    if (code != 200) { [self finish:shader_error(code == 404 ? @"The shader libraries were not found in the repository's latest release." : [NSString stringWithFormat:@"The server answered with status %ld.", (long)code])]; return; }
    NSString *zip = [self.doc stringByAppendingPathComponent:@"msl.download.zip"];   // the temporary file goes away when this method returns: move it out now
    NSFileManager *fm = NSFileManager.defaultManager; NSError *err = nil;
    [fm removeItemAtPath:zip error:nil];
    if (![fm moveItemAtURL:location toURL:[NSURL fileURLWithPath:zip] error:&err]) { [self finish:err]; return; }
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSError *e = unpack_msl(zip, self.doc, ^(double f) { dispatch_async(dispatch_get_main_queue(), ^{ if (!self.finished) self.progress(@"Unpacking", f); }); });
        [fm removeItemAtPath:zip error:nil];
        [self finish:e];
    });
}
- (void)URLSession:(NSURLSession *)s task:(NSURLSessionTask *)t didCompleteWithError:(NSError *)error {
    (void)s; (void)t;
    if (error) [self finish:error];   // a finished download has already gone on to unpacking
}
@end

void dm_shader_download(NSString *documents, DmShaderProgress progress, void (^done)(NSError *_Nullable error)) {
    if (g_downloader) return;   // one at a time
    g_cancel = 0;
    DmShaderDownloader *d = [DmShaderDownloader new]; g_downloader = d;
    d.doc = documents; d.progress = progress; d.done = done;
    NSURLSessionConfiguration *c = NSURLSessionConfiguration.defaultSessionConfiguration;
    c.timeoutIntervalForRequest = 30; c.timeoutIntervalForResource = 3600; c.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
    NSOperationQueue *q = [NSOperationQueue new]; q.maxConcurrentOperationCount = 1;
    d.session = [NSURLSession sessionWithConfiguration:c delegate:d delegateQueue:q];
    d.task = [d.session downloadTaskWithURL:[NSURL URLWithString:kShadersURL]];
    fprintf(stderr, "boot: downloading shader libraries (%s)\n", kShadersURL.UTF8String);
    [d.task resume];
    progress(@"Downloading", -1);
}
void dm_shader_download_cancel(void) {
    DmShaderDownloader *d = g_downloader; if (!d) return;
    g_cancel = 1; [d.task cancel];   // while unpacking, the loop sees the flag
}
