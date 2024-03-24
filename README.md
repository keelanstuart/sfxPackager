# sfxPackager
**A light-weight, self-extracting install creation utility for Windows**

_Copyright (C) 2013-2024 Keelan Stuart (keelanstuart@gmail.com)_

### Features
* Light-weight, self-extracting install creator
* Project-based; create your install project once, then re-build when your files have changed without any modifications
* Easy drag'n'drop user experience
* Integrated JavaScript system, featuring built-in code editor, with extensive installer-related API to help get the job done
* Robust property system that you can initialize in scripts, allow the user to modify in your configuration dialog, and then check during install
* Recognizes and replaces environment variables and registry keys in paths
* Custom HTML content displayable in your welcome (and license) dialog - either in-line or from a file you specify
* Embed URLs as the file source to pull content from the web, rather than storing it statically in your package
* Disk spanning, allowing installs to be partitioned into discretely-size chunks for easy distribution with multiple media sources
* Tools to facilitate License validation
* Unicode used throughout
* 64-bit; very large archives are no problem, though for self-contained installers, a limit of 4GB is implied (but not enforced!)

View the [user's guide](https://docs.google.com/presentation/d/e/2PACX-1vRAVGjiJbSYUrOWB8jEzqG7hMwVbZqvCiAbVmOeL25hoEmN909H-BtGjEawmTMZLta5qHfhGydWDqQd/pub?start=false&loop=false&delayms=30000) to get started!

--------------------------
The user's guide can be of great help in deeper understanding of install creation, but for simple operation, just
drag your files from Windows explorer onto the file list in your sfxpp project. The files will show up there. 
If you included a folder, you will be prompted as to whether you desire a live file system or a fixed one... 
meaning: do you want the directory to be re-examined when your project is built and any new files included?
Conversely, do you want the currently existing files only, meaning that if you add files in the future they will 
not be automatically included? I took this idea, notionally, from the venerable (and discontinued)
PackageForTheWeb from InstallShield.

More complicated operations can be done with the integrated JavaScript-ish capability. You can write scripts
that run during initialization, per file, and at successful completion. A brief overview of the functions available is
in the guide.

I'll be happy to answer any questions.

--------------------------
Special thanks to...

***

Ariya Hidayat, who wrote the awesome FastLZ de-/compressor code. The license in that source is separate, but identical to the MIT license that sfxPackager is released under.

FastLZ - lightning-fast lossless compression library
Portions Copyright (C) 2005-2007 Ariya Hidayat (ariya@kde.org)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

***

Gordon Williams, who wrote the original tiny-js (https://github.com/gfwilliams/tiny-js) ...
... I did modify this version to support TCHAR (compile-time MBCS v. Unicode support), etc.

tiny-js - Extremely simple (~2000 line) JavaScript interpreter
Portions Copyright (c) 2019 Gordon Williams

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
