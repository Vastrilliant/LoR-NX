// Downloads the shader libraries (nxnative-shaders.zip of the repository's latest release) and puts its msl/ folder into the app's Documents (ShaderDownload.m). Used by the setup screen (front_ios.m).
#import <Foundation/Foundation.h>
NS_ASSUME_NONNULL_BEGIN
typedef void (^DmShaderProgress)(NSString *phase, double fraction);   // phase "Downloading" or "Unpacking"; fraction 0...1, or -1 when the size is not known. Called on the main queue
void dm_shader_download(NSString *documents, DmShaderProgress progress, void (^done)(NSError *_Nullable error));   // done is called once on the main queue; error NSURLErrorCancelled after dm_shader_download_cancel
void dm_shader_download_cancel(void);
NS_ASSUME_NONNULL_END
