@echo off
REM 合并四张PNG为RGBA

set "R_IMG=white-marble_ao.png"
set "G_IMG=white-marble_roughness.png"
set "B_IMG=white-marble_metallic.png"
set "A_IMG=white-marble_height.png"
set "OUT_IMG=result.png"

set "R_GRAY=__r_gray.png"
set "G_GRAY=__g_gray.png"
set "B_GRAY=__b_gray.png"
set "A_GRAY=__a_gray.png"

magick "%R_IMG%" -colorspace gray "%R_GRAY%"
magick "%G_IMG%" -colorspace gray "%G_GRAY%"
magick "%B_IMG%" -colorspace gray "%B_GRAY%"
magick "%A_IMG%" -colorspace gray "%A_GRAY%"

REM 重点：直接用四通道合成
magick "%R_GRAY%" "%G_GRAY%" "%B_GRAY%" "%A_GRAY%" -combine -alpha set "%OUT_IMG%"

del "%R_GRAY%"
del "%G_GRAY%"
del "%B_GRAY%"
del "%A_GRAY%"

echo 合成完成，输出文件：%OUT_IMG%
pause