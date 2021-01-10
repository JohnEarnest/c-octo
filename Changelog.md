Changelog
=========

1.0:
----
- initial release

1.1:
----
- new language feature- :unpack long
- new language feature- :pointer
- performance tweaks and build guide information for the OLPC
- escape sequences in string literals now have a special highlight color
- improved error messages when register aliases and labels collide
- added Ctrl+/ to toggle commenting in the text editor
- added Tab/Shift+Tab to block indent/unindent in the text editor
- bug fix- syntax highlighting for multiline strings did not work reliably
- bug fix- corrected a regression in support for negative hex and binary literals
- bug fix- text editor scroll is now reset after loading a new program
- bug fix- some cartridge files were being corrupted on Windows
- bug fix- octo-cli couldn't extract source code from octocarts if they didn't compile
- bug fix- the last button in a menu did not always consume the remaining vertical space
