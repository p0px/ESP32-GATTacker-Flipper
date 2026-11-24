#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void Canvas;
typedef void DialogsApp;
typedef void Expansion;
typedef void File;
typedef void FuriHalSerialHandle;
typedef void FuriMessageQueue;
typedef void FuriMutex;
typedef void FuriStreamBuffer;
typedef void Gui;
typedef void Storage;
typedef void ViewPort;
typedef enum {InputTypePress, InputTypeRelease, InputTypeShort, InputTypeRepeat} InputType;
typedef enum {InputKeyUp, InputKeyDown, InputKeyRight, InputKeyLeft,
              InputKeyOk, InputKeyBack} InputKey;
typedef struct { InputType type; InputKey key; } InputEvent;
uint32_t furi_get_tick(void);
uint32_t furi_ms_to_ticks(uint32_t milliseconds);
