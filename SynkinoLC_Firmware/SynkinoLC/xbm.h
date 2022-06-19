#pragma once

#define busybee_xbm_width  15
#define busybee_xbm_height 15
static const unsigned char busybee_xbm_bits[] U8X8_PROGMEM = {
   0x10, 0x00, 0x10, 0x3C, 0x00, 0x46, 0x60, 0x43, 0x63, 0x21, 0x98, 0x51,
   0xD8, 0x2A, 0x60, 0x07, 0xB8, 0x1A, 0xCC, 0x3F, 0x86, 0x06, 0x42, 0x7B,
   0x22, 0x1B, 0x52, 0x6A, 0x2C, 0x28 };

#define play_xbm_width  9
#define play_xbm_height 9
static const unsigned char play_xbm_bits[] U8X8_PROGMEM = {
   0x03, 0x00, 0x0F, 0x00, 0x3F, 0x00, 0xFF, 0x00, 0xFF, 0x01, 0xFF, 0x00,
   0x3F, 0x00, 0x0F, 0x00, 0x03, 0x00 };

#define pause_xbm_width  8
#define pause_xbm_height 8
static const unsigned char pause_xbm_bits[] U8X8_PROGMEM = {
   0xE7, 0xE7, 0xE7, 0xE7, 0xE7, 0xE7, 0xE7, 0xE7 };

#define sync_xbm_width 20
#define sync_xbm_height 9
static const unsigned char sync_xbm_bits[] U8X8_PROGMEM = {
   0xFE, 0xFF, 0x07, 0xFF, 0xFF, 0x0F, 0xA7, 0xDA, 0x0C, 0xBB, 0x52, 0x0F,
   0x77, 0x4B, 0x0F, 0x6F, 0x5B, 0x0F, 0x73, 0xDB, 0x0C, 0xFF, 0xFF, 0x0F,
   0xFE, 0xFF, 0x07 };

#define logo_xbm_width  96
#define logo_xbm_height 34
#define logo_xbm_x      14
#define logo_xbm_y      20
static const unsigned char logo_xbm_bits[] U8X8_PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0x03,
  0x00, 0x1f, 0x00, 0xfe, 0xff, 0xff, 0xff, 0x3f, 0x00, 0x00, 0x00, 0x0e,
  0xc0, 0x60, 0x00, 0xff, 0xff, 0xff, 0xff, 0x1f, 0x0e, 0x38, 0xe0, 0x30,
  0x30, 0x08, 0x80, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x0e, 0x38, 0xe0, 0x40,
  0x08, 0x14, 0x9f, 0xff, 0xff, 0xff, 0xff, 0x5f, 0x00, 0x00, 0x00, 0x80,
  0x04, 0x14, 0x8f, 0x0f, 0xec, 0xdb, 0xef, 0x4d, 0x4e, 0x20, 0x3c, 0xc0,
  0x04, 0x14, 0x87, 0xf7, 0xef, 0x9b, 0xef, 0x5d, 0xc4, 0x20, 0x42, 0xc0,
  0x02, 0x14, 0x8b, 0xfb, 0xdf, 0x9d, 0xef, 0x2d, 0xc4, 0x20, 0x42, 0x80,
  0x02, 0x0e, 0x89, 0xfb, 0xdf, 0x5d, 0xef, 0x3d, 0x44, 0x21, 0x81, 0x80,
  0x01, 0x05, 0x90, 0xfb, 0xbf, 0x5e, 0xef, 0x1d, 0x44, 0x21, 0x81, 0x80,
  0x01, 0x05, 0x90, 0xf7, 0x7f, 0xdf, 0xee, 0x15, 0x44, 0x22, 0x81, 0x80,
  0x81, 0x04, 0x90, 0x0f, 0x7f, 0xdf, 0xee, 0x09, 0x44, 0x22, 0x81, 0x40,
  0x81, 0x1e, 0x90, 0xff, 0x7e, 0xdf, 0xed, 0x19, 0x44, 0x24, 0x81, 0x40,
  0x81, 0x26, 0x90, 0xff, 0x7d, 0xdf, 0xeb, 0x05, 0x44, 0x28, 0x81, 0x40,
  0x92, 0x24, 0x88, 0xff, 0x7d, 0xdf, 0xeb, 0x2d, 0x44, 0x28, 0x81, 0x40,
  0x1a, 0x15, 0x88, 0xff, 0x7d, 0xdf, 0xe7, 0x2d, 0x44, 0x30, 0x42, 0x40,
  0x1c, 0x1e, 0x84, 0xff, 0x7e, 0xdf, 0xe7, 0x5d, 0x44, 0x30, 0x42, 0x40,
  0x1e, 0x05, 0x84, 0x03, 0x7f, 0xdf, 0xef, 0x4d, 0x4e, 0x20, 0x3c, 0x40,
  0x1f, 0x05, 0x82, 0xff, 0xff, 0xff, 0xff, 0x1f, 0x00, 0x00, 0x00, 0x40,
  0x00, 0x82, 0x81, 0xff, 0xff, 0xff, 0xff, 0xcf, 0x00, 0x00, 0x00, 0x40,
  0xc0, 0x60, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x43,
  0x00, 0x1f, 0x00, 0xfe, 0xff, 0xff, 0xff, 0x3f, 0x02, 0xfe, 0x0f, 0x4e,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x1f, 0x70,
  0x00, 0x00, 0x2a, 0x40, 0x00, 0x00, 0x00, 0x00, 0x01, 0xfb, 0x18, 0x40,
  0x00, 0x00, 0x21, 0x40, 0x00, 0x00, 0x00, 0x00, 0x81, 0x7b, 0x3f, 0x20,
  0x00, 0x00, 0x2b, 0x4b, 0x9d, 0x0d, 0x67, 0x07, 0x82, 0x7b, 0x3f, 0x10,
  0x00, 0x00, 0x29, 0xd5, 0x54, 0x14, 0x15, 0x05, 0x9c, 0x7b, 0x3f, 0x0c,
  0x00, 0x00, 0x49, 0x55, 0x5d, 0x54, 0x17, 0x07, 0xa0, 0x7b, 0xbf, 0x03,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x80, 0x7b, 0x3f, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0xc3, 0x18, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x1f, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x0f, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x07, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x01, 0x00
};