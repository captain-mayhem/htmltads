TADS 3 Author's Kit for Windows 95 and NT
-----------------------------------------

The material in this archive is copyrighted.  Please refer to
LICENSE.TXT, which should accompany this archive, for information.

This distribution contains the complete TADS 3 Author's Kit for
Windows 95 and NT.  This kit contains everything you need to write,
debug, and run games for the standard TADS 3 and HTML TADS 3.


---------------------------------------------------------------------------
IMPORTANT NOTE -- COMCTL32.DLL

If HTML TADS crashes when you select the "Options" item of the "View"
menu, you probably have an out-of-date version of ComCtl32.DLL.

HTML TADS makes use of a system DLL called ComCtl32.DLL.  This DLL is
shipped as part of Windows 95 and NT, but HTML TADS is not compatible
with certain older versions of the DLL.  If you have installed
Internet Explorer versions 3 or 4, you'll probably have the correct
version of the DLL; if not, you can download the DLL from the
Microsoft web site.  To determine if you have the correct version,
use the explorer to open your C:\WINDOWS folder (or whichever folder
contains your Windows system files), and open the SYSTEM folder (or
SYSTEM32 folder on Windows NT).  Right-click on the file
COMCTL32.DLL, and select the "Properties" menu item.  Windows will
display the properties dialog for the file.  Click on the "Version"
tab in the dialog.  Look at the "File version" string - this will
look something like "4.71.1712.3".  Make sure that this version
starts with at least "4.70" - if it's lower, you have an older
version of the DLL that won't work correctly with HTML TADS.  You can
get the newer version from the Microsoft web site.  Download this
file:

   ftp://ftp.microsoft.com/softlib/mslfiles/com32upd.exe

After downloading the file, double-click it to run the installation
program, which will install the upgraded DLL for you.  Note that HTML
TADS automatically checks to make sure you have the correct version
installed and warns you at startup if you don't.  Although you can
choose to continue running after this warning, we strongly encourage
you to upgrade your ComCtl32.DLL before running HTML TADS.

I'd like to thank Klywarre (The Avatar) [aka: James Mallette] for
tracking down this DLL problem.

