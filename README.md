# SALVL2RBX
Convert SALVLs to Roblox models

## How to use
You'll need two things in order to use this program.
- Roblox Studio Mod Manager (https://github.com/MaximumADHD/Roblox-Studio-Mod-Manager)
- SATools (https://github.com/X-Hax/sa_tools)

First, you'll need to get SA1LVLs of the stages you want to import, I use the Dreamcast Conversion maps, which has the SA1LVLs in `system/data/`.

Then, you'll need to extract the relevant textures using Texture Editor from SATools (Tools > General Tools > Texture Editor). These are PVM and/or PRS files in `system/`. Export texture pack via Texture Editor (File > Export texture pack).

Refer to these links to see which stage ids and texture packs refer to which stages:
- http://info.sonicretro.org/SCHG:Sonic_Adventure/Level_Data_Locations
- http://info.sonicretro.org/SCHG:Sonic_Adventure/Textures

Then you want to compile SALVL2RBX (Visual Studio only) and call the resulting exe with these parameters.

`SALVL2RBX upload/content_directory scale sa1lvl texlist_index_txt`

Argument | Function
--------|--------
`upload/content_directory` | If `upload`, this will upload to the Roblox account associated with Roblox Studio, and output to `salvl/` where the command was issued. Otherwise, it's a path where the program will output to, such as Roblox Studio Mod Manager's content directory (`C:\Users\USER\AppData\Roaming\RbxModManager\ModFiles\content`).
`scale` | Scale of the map when importing into Roblox. Recommended 0.455
`sa1lvl` | Path to the sa1lvl
`texlist_index_txt` | Path to the index.txt of the extracted texture pack

# WARNING
By using upload mode, you consent to two terms.
 1. This program will retrieve your Roblox Studio session (ROBLOSECURITY) to upload assets onto Roblox. Note that this program does not communicate to any servers other than Roblox's.
 2. Roblox may, whether deserved or not, take moderation action against your account for the assets uploaded. Please don't run this logged into your main account.
