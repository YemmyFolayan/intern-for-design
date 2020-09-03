# Intern Project Demo

This project is for showing how the AutoFlip works and rendering cropped user input videos.
This demo uses fixed workers for processing ffmpeg and autoflip, 4 workers for ffmpeg, 1 for autoflip.
The UI is designed to show the video as well as cropping parameters of Autoflip.

## Installation

The project requires NodeJs, TypeScript and NPM installed.

## Build

Inside folder of selected demo web_autoflip_demo/autoflip_demo_UI

```
cd  web_autoflip_demo/autoflip_demo_UI
```

```
npm i

```

To build the code

```
npm run build
```

To run a local web server

```
npm run serve
```

To run all the tests on test folder

```
npm run test
```

## Usage

Open the autoflip.html inside the folder to see the project.

Upload video file:

1. Drag and drop file in drag&drop section.
2. Click button 'select file' to upload a file.
3. Click button 'try our demo' to upload a demo video.

Wait for progress bar to finish.

Play with video control and control buttons.
