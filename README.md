# sfxPackager
A light-weight, self-extracting install creation utility for Windows

Copyright (C) 2013-2019 Keelan Stuart (keelanstuart@gmail.com)

--------------------------
A full users manual is unavailable at this time (though it may be forthcoming soon), but for simple operation,
drag your files from Windows explorer onto the file list in your sfxpp project. The files will show up there and, 
if you included a folder, you will be prompted as to whether you desire a live file system or a fixed one... 
meaning, do you want the directory to be re-examined when your project is built and any new files included?
Conversely, do you want the currently existing files only, meaning that if you add files in the future they will 
not be automatically included? I took this idea, notionally, from the venerable (and discontinued)
PackageForTheWeb from InstallShield.

More complicated operations can be done with the integrated JavaScript-ish capability. You can write scripts
that run during initialization, per file, and at successful completion. If you want to see what functions are available,
you'll currently have to look in the code.

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