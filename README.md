# SPDIF Logic Analyzer

This plugin is for use in the [Saleae Logic](http://www.saleae.com/downloads) program to decode captured data from any SPDIF Interface.

LICENSE is GPLv2

So far I've built/run it on 64 bit windows and linux.

## Installation

To build the plugin, change to the plugin directory and run `python build_analyzer.py`.

Once built (look for any errors in the build output), open Saleae Logic and Click

Options > Preferences > "Developer" Tab

The only option should be an area to specify a folder. Point this to the `release` folder in this plugin directory. If you already have a generic user-directory to collect plugins, copy `release/libspdifAnalyzer.so` to your folder. Other architectures may use other file extensions (Mac: dylib; Linux: so; Windows: dll).

## Features

- Auto-clocking, will track changes in clock speed.
- Marks "B" frame boundaries with a white Dod
- Marks out-of-sequence "B" frames with a red Dot
- Marks non-decodable gaps in SPDIF interface with red X
- WAV Output, save the capture to a wave file (48.0 kHz only)
- RAW Output, save all 32-bit words from the interface
- Errors show in data table

## Use

Install and assign to the SPDIF wire.

For reliable capture of 48kHz spdif, capture at 25 MHz
For reliable capture of 192kHz spdif, capture at 100 MHz

![wave_view](images/spdif_wave_view.png)
![decoder_view](images/spdif_decoder_view.png)
![menu](images/spdif_analyzer_menu.png)

## As-is

This software is provided as-is, with no guarantees, so there.

## To-Do

- The internal command-line analyzer (spdif.c) detects far more errors than the UI and it would be good to see these capabilities brought out.
- Needs a channel-status and validity bits reported in the data table, ideally only changes would be marked.
- Add a realistic signal generator

## Analyzer SDK

Documentation for the Saleae Logic Analyzer SDK can be found here:
https://github.com/saleae/SampleAnalyzer

That documentation includes:

- Detailed build instructions
- Debugging instructions
- Documentation for CI builds
- Details on creating your own custom analyzer plugin

## Installation Instructions

To use this analyzer, simply download the latest release zip file from this github repository, unzip it, then install using the instructions found here:
https://support.saleae.com/faq/technical-faq/setting-up-developer-directory

## Publishing Releases

This repository is setup with Github Actions to automatically build PRs and commits to the master branch.
However, these artifacts automatically expire after a time limit.
To create and publish cross-platform releases, simply push a commit tag. This will automatically trigger the creation of a release, which will include the Windows, Linux, and MacOS builds of the analyzer.

## A note on downloading the MacOS Analyzer builds

This section only applies to downloaded pre-built protocol analyzer binaries on MacOS. If you build the protocol analyzer locally, or acquire it in a different way, this section does not apply.
Any time you download a binary from the internet on a Mac, wether it be an application or a shared library, MacOS will flag that binary for "quarantine". MacOS then requires any quarantined binary to be signed and notarized through the MacOS developer program before it will allow that binary to be executed.
Because of this, when you download a pre-compiled protocol analyzer plugin from the internet and try to load it in the Saleae software, you will most likely see an error message like this:

> "libSimpleSerialAnalyzer.so" cannot be opened because th developer cannot be verified.
> Signing and notarizing of open source software can be rare, because it requires an active paid subscription to the MacOS developer program, and the signing and notarization process frequently changes and becomes more restrictive, requiring frequent updates to the build process.
> The quickest solution to this is to simply remove the quarantine flag added by MacOS using a simple command line tool.
> Note - the purpose of code signing and notarization is to help end users be sure that the binary they downloaded did indeed come from the original publisher and hasn't been modified. Saleae does not create, control, or review 3rd party analyzer plugins available on the internet, and thus you must trust the original author and the website where you are downloading the plugin. (This applies to all software you've ever downloaded, essentially.)
> To remove the quarantine flag on MacOS, you can simply open the terminal and navigate to the directory containing the downloaded shared library.
> This will show what flags are present on the binary:

```sh
xattr libSimpleSerialAnalyzer.so
# example output:
# com.apple.macl
# com.apple.quarantine
```

This command will remove the quarantine flag:

```sh
xattr -r -d com.apple.quarantine libSimpleSerialAnalyzer.so
```

To verify the flag was removed, run the first command again and verify the quarantine flag is no longer present.

### Building your Analyzer

CMake and a C++ compiler are required. Instructions for installing dependencies can be found here:
https://github.com/saleae/SampleAnalyzer

The fastest way to use this analyzer is to download a release from github. Local building should only be needed for making your own changes to the analyzer source.

#### Windows

```bat
mkdir build
cd build
cmake .. -A x64
cmake --build .
:: built analyzer will be located at SampleAnalyzer\build\Analyzers\Debug\SimpleSerialAnalyzer.dll
```

#### MacOS

```bash
mkdir build
cd build
cmake ..
cmake --build .
# built analyzer will be located at SampleAnalyzer/build/Analyzers/libSimpleSerialAnalyzer.so
```

#### Linux

```bash
mkdir build
cd build
cmake ..
cmake --build .
# built analyzer will be located at SampleAnalyzer/build/Analyzers/libSimpleSerialAnalyzer.so
```
