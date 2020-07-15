An explanation of the Mozilla Source Code Directory Structure and links to
project pages with documentation can be found at:

    https://developer.mozilla.org/en/Mozilla_Source_Code_Directory_Structure

For information on how to build Mozilla from the source code, see:

    http://developer.mozilla.org/en/docs/Build_Documentation

To have your bug fix / feature added to Mozilla, you should create a patch and
submit it to Bugzilla (http://bugzilla.mozilla.org). Instructions are at:

    http://developer.mozilla.org/en/docs/Creating_a_patch
    http://developer.mozilla.org/en/docs/Getting_your_patch_in_the_tree

If you have a question about developing Mozilla, and can't find the solution
on http://developer.mozilla.org, you can try asking your question in a
mozilla.* Usenet group, or on IRC at irc.mozilla.org. [The Mozilla news groups
are accessible on Google Groups, or news.mozilla.org with a NNTP reader.]

You can download nightly development builds from the Mozilla FTP server.
Keep in mind that nightly builds, which are used by Mozilla developers for
testing, may be buggy. Firefox nightlies, for example, can be found at:

    ftp://ftp.mozilla.org/pub/firefox/nightly/latest-trunk/

No, mozilla does not support xulrunner anymore.

-------------------------------------------------------------

This will be Xulrunner IceFox Widget, it is Not Firefox, not Iceweasel, not Seamonkey, not PaleMoon, it is just Xulrunner XML2HTML App platform, which Mozilla abandoned.
IceFox is like combination of two words, which somebody understands as arctic fox animal.

WHY THIS

Because Mozilla removed most of info about Xulrunner and sources from their website and services, after gaming with CVS->Mercurial, Ftp and Git.
(We can understand Mozilla, because Python2.7 != Python3+ problem is ugly and simple rewriting code on own Rust is simpler.)

IceFox supports XUL and NPAPI. If you want also XPCOM, you can see on PaleMoon Browser.

WHO SHOULD USE IT CURRENTLY

Nobody from regular end-users. They should prefer Chrome (HTML5+PPAPI+V8+WebExtensions), PaleMoon (HTML5+NANPI+XUL+XPCOM), Firefox (no NPAPI, no PPAPI, no stable WebExtensions), Safari (Chrome parent), Egde (Chrome clone), Kiwi (Chrome fork), Opera (Chrome clone), Yandex (Chrome fork) and Sputnik (Chrome clone) browsers.

WHERE TO BUILD

Use old ubuntu or debian distro.

HOW TO INSTALL DEPS

sudo apt-get install aptitude synaptic
sudo apt-get install  libasound2 libasound2-dev libcurl3 libcurl4-openssl-dev libssl0.9.8 libssl-dev  libgtk2.0-dev autoconf2.13 yasm libxt-dev zlib1g-dev libsqlite3-dev libbz2-dev libpulse-dev libgconf2-dev libx11-dev libx11-xcb-dev libgtk2.0-dev zip vim-gtk  lua5.1 liblua5.1-dev libncurses5-dev libxml2-dev libxslt1-dev
sudo aptitude install build-essential git git-core gitk htop perl curl wget patch
sudo apt-get install  libasound2 libasound2-dev libcurl3 libcurl4-openssl-dev libssl0.9.8 libssl-dev  libgtk2.0-dev autoconf2.13 yasm libxt-dev zlib1g-dev libsqlite3-dev libbz2-dev libpulse-dev libgconf2-dev libx11-dev libx11-xcb-dev libgtk2.0-dev zip vim-gtk  lua5.1 liblua5.1-dev libncurses5-dev libxml2-dev libxslt1-dev build-essential libpthread-stubs0-dev


HOW TO BUILD

./configure --enable-application=browser --enable-default-toolkit=cairo-gtk2 --enable-optimize="-O1" --disable-jemalloc  --disable-xpcom-obsolete --disable-libnotify --disable-dbus --disable-necko-wifi --with-curl --disable-javaxpcom  --disable-jsd --disable-jsloader --disable-gamepad  --disable-crashreporter --disable-updater --disable-debug --disable-tests --enable-oficial-branding --with-pthreads --without-system-nspr --enable-old-abi-compat-wrappers

make
chmod a+rx ./build/unix/run-mozilla.sh
chmod a+rx ./dist/bin/run-mozilla.sh
make

HOW TO INSTALL

sudo make install

HOW TO CHECK

ls -lsahF /usr/local/bin/*fox
