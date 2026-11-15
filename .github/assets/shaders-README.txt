LoR NX - installing the shader libraries and romfs.img
======================================================

Both go into the app's Documents folder. In the Files app that is
On My iPhone > (the app's name, "Library of Ruina" unless you changed it).
If the app's folder is not listed, install the IPA, open the app once,
close it and look again.

Final layout:

    Documents/
        msl/            
            msl.id
            b-XXXXXX/
        romfs.img      


1. Shader libraries (the msl folder)

   - This folder contains all the metal libraries

   - Copy the whole msl folder (not this README) into the app's folder.
   - If there already is an msl folder there, delete it first.
   - Wait for the copy to finish. The app ignores an incomplete copy.
   - They must come from the same build as the IPA: the app only uses
     them when msl.id matches the one the IPA was built with. After a
     new build, copy the new msl folder over the old one.


2. romfs.img 

   - This file holds all of the game assets

   - Copy the extracted romfs.img into the app's folder, next to msl.
   - The name must be exactly romfs.img, in lowercase.
   - It is needed once per game version. A new IPA build does not need a
     new romfs.img unless the game files changed; if the image is not the
     one the IPA was built for, the app says there is no game file image.


You can download the romfs.img in the Discord: 
https://discord.gg/nRmztE2unw