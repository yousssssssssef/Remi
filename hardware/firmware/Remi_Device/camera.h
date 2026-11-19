#pragma once
#include <stddef.h>
#include <stdint.h>

void cameraBegin();                                  // sets data.hasCamera
bool cameraCaptureJpeg(uint8_t **out, size_t *len);  // backend task; caller frees *out
void cameraIdle();                                   // call from loop(): powers the camera down when unused
void audioPause(bool paused);                        // the sketch stops I2S while the camera owns IO0 (CAM board)

// A photo sent from the phone remote, waiting to be read (owned by remote.cpp until the scan takes it).
struct Photo { uint8_t *bytes; size_t len; bool png; };
bool takePhoto(Photo &photo);                        // moves the pending photo out; caller frees photo.bytes
