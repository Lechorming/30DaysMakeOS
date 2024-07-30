
TOOLPATH = ../../z_tools/

MAKE     = $(TOOLPATH)make.exe -r
BIM2BIN  = $(TOOLPATH)bim2bin.exe -osacmp

# 默认规则

default :
	$(MAKE) ayanami.jpg
	$(MAKE) yui.jpg
	$(MAKE) GZHU.jpg

# 文件生成规则
	
ayanami.jpg: ayanami_.jpg
	$(BIM2BIN) in:ayanami_.jpg out:ayanami.jpg
	
yui.jpg: yui_.jpg
	$(BIM2BIN) in:yui_.jpg out:yui.jpg

GZHU.jpg: GZHU_.jpg
	$(BIM2BIN) in:GZHU_.jpg out:GZHU.jpg

fijisan.jpg: fijisan_.jpg
	$(BIM2BIN) in:fijisan_.jpg out:fijisan.jpg
	
night.bmp: night_.bmp
	$(BIM2BIN) in:night_.bmp out:night.bmp

%.jpg: %.jpg
	$(BIM2BIN) in:$*.jpg out:$*.jpg
	
%.bmp: %.bmp
	$(BIM2BIN) in:$*.bmp out:$*.bmp


