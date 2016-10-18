PLUGIN_NAME = spike_triggered_average

RTXI_INCLUDES = 

HEADERS = spike-triggered-average.h\

SOURCES = spike-triggered-average.cpp \
          moc_spike-triggered-average.cpp\

LIBS = -lrtplot

### Do not edit below this line ###

include $(shell rtxi_plugin_config --pkgdata-dir)/Makefile.plugin_compile
