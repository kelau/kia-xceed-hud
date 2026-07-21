param(
    [Parameter(Mandatory = $true)]
    [string]$LibraryRoot
)

$header = Join-Path $LibraryRoot 'src\databus\Arduino_ESP32RGBPanel.h'
$source = Join-Path $LibraryRoot 'src\databus\Arduino_ESP32RGBPanel.cpp'
if (!(Test-Path $header) -or !(Test-Path $source)) { throw "Arduino_GFX RGB panel sources not found at $LibraryRoot" }

$h = Get-Content $header -Raw
if ($h -notmatch 'getFrameBuffers') {
    $h = $h.Replace(
        '  uint16_t *getFrameBuffer(int16_t w, int16_t h);',
        "  uint16_t *getFrameBuffer(int16_t w, int16_t h);`r`n  bool getFrameBuffers(uint16_t **front, uint16_t **back);`r`n  bool switchFrameBuffer(uint16_t *frame_buffer, int16_t w, int16_t h);"
    ).Replace(
        '  esp_lcd_panel_handle_t _panel_handle = NULL;',
        "  esp_lcd_panel_handle_t _panel_handle = NULL;`r`n  uint16_t *_frame_buffers[2] = {NULL, NULL};"
    )
    Set-Content $header $h -NoNewline
}

$cpp = Get-Content $source -Raw
$guardBlock = "  if (_panel_handle)`n  {`n    return _frame_buffers[0];`n  }"
$cpp = [regex]::Replace($cpp, '(?s)(  if \(_panel_handle\)\s*\{\s*return _frame_buffers\[0\];\s*\}\s*){2,}', $guardBlock + "`n", 1)
if ($cpp -notmatch 'Arduino_ESP32RGBPanel::getFrameBuffers') {
    $cpp = [regex]::Replace($cpp, '(uint16_t \*Arduino_ESP32RGBPanel::getFrameBuffer\(int16_t w, int16_t h\)\s*\{)', ('$1' + "`n  if (_panel_handle)`n  {`n    return _frame_buffers[0];`n  }"), 1)
    $cpp = $cpp.Replace('.num_fbs = 1,', '.num_fbs = 2,')
    $cpp = [regex]::Replace($cpp, 'void \*frame_buffer = nullptr;\s*ESP_ERROR_CHECK\(esp_lcd_rgb_panel_get_frame_buffer\(_panel_handle, 1, &frame_buffer\)\);\s*return \(\(uint16_t \*\)frame_buffer\);', "void *front = nullptr;`n  void *back = nullptr;`n  ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(_panel_handle, 2, &front, &back));`n  _frame_buffers[0] = (uint16_t *)front;`n  _frame_buffers[1] = (uint16_t *)back;`n  return _frame_buffers[0];", 1)
    $methods = @'

bool Arduino_ESP32RGBPanel::getFrameBuffers(uint16_t **front, uint16_t **back)
{
  if (!_panel_handle || !_frame_buffers[0] || !_frame_buffers[1] || !front || !back) return false;
  *front = _frame_buffers[0];
  *back = _frame_buffers[1];
  return true;
}

bool Arduino_ESP32RGBPanel::switchFrameBuffer(uint16_t *frame_buffer, int16_t w, int16_t h)
{
  if (!_panel_handle || (frame_buffer != _frame_buffers[0] && frame_buffer != _frame_buffers[1])) return false;
  return esp_lcd_panel_draw_bitmap(_panel_handle, 0, 0, w, h, frame_buffer) == ESP_OK;
}

'@
    $guard = '#endif // #if defined(ESP32) && (CONFIG_IDF_TARGET_ESP32S3)'
    $cpp = $cpp.Replace($guard, $methods + $guard)
}
Set-Content $source $cpp -NoNewline

if ((Get-Content $header -Raw) -notmatch 'getFrameBuffers' -or (Get-Content $source -Raw) -notmatch 'Arduino_ESP32RGBPanel::getFrameBuffers' -or (Get-Content $source -Raw) -notmatch 'num_fbs = 2') {
    throw 'Arduino_GFX double-buffer patch did not apply cleanly'
}
Write-Host 'Arduino_GFX RGB double buffering is enabled.'
