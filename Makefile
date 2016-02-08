PLUGIN_NAME = spike_triggered_average

RTXI_INCLUDES = /usr/local/lib/rtxi_includes

HEADERS = spike-triggered-average.h\
          $(RTXI_INCLUDES)/scrollbar.h\
          $(RTXI_INCLUDES)/scrollzoomer.h\
          $(RTXI_INCLUDES)/basicplot.h\

SOURCES = spike-triggered-average.cpp \
          moc_spike-triggered-average.cpp\
          $(RTXI_INCLUDES)/basicplot.cpp\
          $(RTXI_INCLUDES)/scrollbar.cpp\
          $(RTXI_INCLUDES)/scrollzoomer.cpp\
          $(RTXI_INCLUDES)/moc_scrollbar.cpp\
          $(RTXI_INCLUDES)/moc_scrollzoomer.cpp\
          $(RTXI_INCLUDES)/moc_basicplot.cpp\

LIBS = -lqwt

### Do not edit below this line ###

include $(shell rtxi_plugin_config --pkgdata-dir)/Makefile.plugin_compile
