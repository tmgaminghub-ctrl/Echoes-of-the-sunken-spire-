ECHOES OF THE SUNKEN SPIRE
Windows x64 portable build  (native C++ port)

====================================================================
 TO PLAY
====================================================================
 Double-click  Echoes-of-the-Sunken-Spire.exe .

 That's it. There is no installer, no setup and no assets folder:
 every model, texture, animation and sound is baked into the
 executable. You can move the .exe to a USB stick, a desktop or any
 folder and it will still run.


 Requires 64-bit Windows 7 or newer and a mouse. An Xbox-compatible
 controller is optional and works at the same time as keyboard+mouse.

====================================================================
 IF WINDOWS BLOCKS IT ("Windows protected your PC")
====================================================================
 The game is not code-signed (a certificate costs money and cannot
 be issued to a hobby build), so Windows will warn the first time.
 Nothing about the warnings means the file is broken.

 A) "Windows protected your PC"  (blue box, SmartScreen)
    Click  More info  ->  Run anyway .
    If there is no "More info" link, use option B.

 B) File is blocked and will not run at all
    Right-click the .exe -> Properties -> at the bottom of the
    General tab tick  Unblock  -> Apply -> OK.
    (Or in PowerShell:  Unblock-File .\Echoes-of-the-Sunken-Spire.exe )

 C) Windows Defender deleted or quarantined it
    Defender -> Virus & threat protection -> Protection history
    -> find the entry -> Actions -> Allow on device.
    Then add the folder: Virus & threat protection -> Manage
    settings -> Exclusions -> Add an exclusion -> Folder.

 D) "You need to be an administrator" / it asks to make changes
    This build declares asInvoker, so it should never ask. If you
    are running an older copy from before that fix, re-download.

 TIP: launching from a command prompt avoids the SmartScreen prompt
 entirely (SmartScreen only filters Explorer/double-click launches):
       Echoes-of-the-Sunken-Spire.exe

====================================================================
 CONTROLS
====================================================================
 WASD ......... move                LMB ......... attack (combo)
 SHIFT ........ sprint              RMB ......... block
 SPACE ........ dodge roll          Q ........... class ability
 E ............ drink elixir        F ........... interact
 TAB / MMB .... lock on             ESC ......... pause
 MOUSE / arrows camera
 Left stick / right stick / A / B / X / Y / LB / RB / Start also work.

 After the title screen, choose one of five classes:
   1 Duelist    rapier + buckler, balanced   Q: Riposte
   2 Warden     sword + shield, tanky        Q: Shield Bash
   3 Ember Mage staff, fragile, fast mana    Q: Emberbolt
   4 Ranger     bow, fastest, ranged         Q: Piercing Shot
   5 Berserker  greataxe, heavy hitter       Q: Whirlwind
 Arrow keys / A-D / 1-5 to pick, Enter to begin. Every class has 3 elixirs (E).

====================================================================
 COMMAND LINE (optional)
====================================================================
   --width 1920 --height 1080     window size
   --render-width 1280 --render-height 720   internal resolution
   --play                         skip the title + class picker
   --class duelist|warden|mage|ranger|berserker   pick a class
   --game spire
   --help                         list every option

====================================================================
 CREDITS / LICENCES
====================================================================
 3D assets by Kay Lousberg - KayKit Adventurers, Skeletons,
 Halloween Bits, Character Animations, Dungeon, Forest, Platformer
 and Fantasy Weapons Bits, released CC0.
 Full licence texts are in the licences folder next to this file.
 Engine and game code: the KayKit Native port (C++17, no engine,
 no SDL, no OpenGL - a hand-written software rasterizer).
