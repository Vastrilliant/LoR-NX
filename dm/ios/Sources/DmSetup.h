// What the first-run setup screen (front_ios.m) is waiting for, decided at start by main.m (setup_check) and again after a download (front_ios_setup_recheck).
// Shader problems come first: the app can fix those itself (ShaderDownload.m), while the RomFS has to come from the player.
#pragma once

enum {
    DM_SETUP_OK = 0,
    DM_SETUP_SHADERS_MISSING,      // no Documents/msl (or an empty one)
    DM_SETUP_SHADERS_INCOMPLETE,   // Documents/msl without its msl.id, or with fewer files / bytes than msl.id lists (a copy that stopped partway)
    DM_SETUP_SHADERS_OTHER_BUILD,  // Documents/msl is complete but belongs to another build of the app
    DM_SETUP_ROMFS_MISSING,        // no romfs.img in Documents (and none in the app)
    DM_SETUP_ROMFS_CORRUPT,        // romfs.img is there but cut off, invalid or not the image this build needs
    DM_SETUP_ROMFS_ZIPPED,         // romfs.img (or romfs.zip) is a zip archive
    DM_SETUP_ROMFS_7Z,             // romfs.img (or romfs.7z, romfs.img.7z) is a 7z archive
};

int front_ios_setup_problem(void);    // one of the above; DM_SETUP_OK when the game can start
int front_ios_setup_recheck(void);    // looks at Documents again (after a download or an import) and returns the new problem
