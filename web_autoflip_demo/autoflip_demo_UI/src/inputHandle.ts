/**
 * Copyright 2020 Google LLC
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import {
  curAspectRatio,
  numberOfSection,
  cropWindowStorage,
  cropHandlerStorage,
  signalHandlerStorage,
  borderHandlerStorage,
  sectionIndexStorage,
} from './globals';
import { inputAspectWidth, inputAspectHeight } from './globals_dom';
import { addHistoryButton } from './videoHandle';

/** Handles inputs to create history button and initialize all storage arrays. */
export function handleInput(): boolean {
  // Reads the user inputs for aspect ratio;
  const inputHeight = inputAspectHeight.value;
  const inputWidth = inputAspectWidth.value;
  if (Number(inputHeight) === 0 || Number(inputWidth) === 0) {
    alert('Please enter positive number greater then 0');
    return false;
  }
  curAspectRatio.inputWidth = Number(inputWidth);
  curAspectRatio.inputHeight = Number(inputHeight);
  addHistoryButton();
  let finished: boolean[] = [];
  for (let i = 0; i < numberOfSection; i++) {
    finished[i] = false;
  }
  cropWindowStorage[
    `${curAspectRatio.inputHeight}&${curAspectRatio.inputWidth}`
  ] = [];
  cropHandlerStorage[
    `${curAspectRatio.inputHeight}&${curAspectRatio.inputWidth}`
  ] = [];
  signalHandlerStorage[
    `${curAspectRatio.inputHeight}&${curAspectRatio.inputWidth}`
  ] = [];
  borderHandlerStorage[
    `${curAspectRatio.inputHeight}&${curAspectRatio.inputWidth}`
  ] = [];
  sectionIndexStorage[
    `${curAspectRatio.inputHeight}&${curAspectRatio.inputWidth}`
  ] = 0;
  return true;
}
