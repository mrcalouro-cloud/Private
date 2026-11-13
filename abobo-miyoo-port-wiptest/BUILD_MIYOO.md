# Building and installing on the Miyoo Mini Plus (Onion OS)

You need: a PC with Docker, ffmpeg and git, and the SD card from the Miyoo.

1. Get the Miyoo Mini cross-compiler (used by the Onion community):
       git clone https://github.com/shauninman/union-miyoomini-toolchain
       cd union-miyoomini-toolchain
       make shell            # builds a Docker image and opens a shell; ./workspace on the PC appears as /root/workspace
2. Unzip this project into that workspace folder (so it appears as /root/workspace/abobo-port or similar).
3. Inside the toolchain shell, build ONE of these (the Miyoo Mini normally ships SDL 1.2, so try the first):
       make miyoo1           # SDL 1.2 + SDL_mixer
       make miyoo            # SDL2 + SDL2_mixer (only if your toolchain has SDL2)
   Both produce a file called `abobo`. If a library is missing the linker will say which one.
4. Outside Docker (on the PC), in the project folder:
       ./package_onion.sh    # needs ffmpeg; creates the AbobosBigAdventure folder (about 110 MB, mostly sound)
5. Copy the AbobosBigAdventure folder to  SD card:/App/  and start it from Onion's Apps menu.

Controls: D-pad = move, A = A key (punch / confirm), B = S key (kick / back), X = Space, Y = D,
Start = pause and click (use Start on the "CLICK ANYWHERE TO PLAY" screen), Select = M (mute),
L1 = Enter, hold R1 = pointer mode (D-pad moves a pink cross, A clicks), Menu = quit.

If it crashes on start, run it from a terminal/telnet and send me the last lines of output.

## No PC tools? Let GitHub build it (free)
1. Make a free GitHub account and a new empty repository.
2. Upload ALL files from this zip (including the hidden .github folder) to the repository.
3. Open the repository's "Actions" tab, pick "Build Miyoo package", press "Run workflow".
4. When it finishes (can take 30+ minutes), download the "AbobosBigAdventure" artifact, unzip it, and copy that folder to SD card:/App/.
This workflow has not been run by me; if a step fails, send me the red error text from the log.
