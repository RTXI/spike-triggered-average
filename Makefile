PLUGIN_NAME = STA

HEADERS = STA.h\
    include/basicplot.h\
    include/scrollbar.h\
    include/scrollzoomer.h

SOURCES = STA.cpp \
    moc_STA.cpp\
    include/basicplot.cpp\
    include/scrollbar.cpp\
    include/scrollzoomer.cpp\
    include/moc_scrollbar.cpp\
    include/moc_scrollzoomer.cpp\
    include/moc_basicplot.cpp

LIBS = -lqwt
    
### Do not edit below this line ###

include $(shell rtxi_plugin_config --pkgdata-dir)/Makefile.plugin_compile
