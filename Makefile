PAHO_MQTT_C_URL := https://github.com/eclipse/paho.mqtt.c
PAHO_MQTT_C_BRANCH := v1.3.9
PAHO_MQTT_C_REPODIR := paho.mqtt.c
PAHO_MQTT_C_BUILDDIR := paho.mqtt.c.build
PAHO_MQTT_C_USRDIR := paho.mqtt.c.usr
PAHO_MQTT_C_BLDFLAGS = blddir=$(abspath $(PAHO_MQTT_C_BUILDDIR))

PAHOLIBDIR := $(PAHO_MQTT_C_USRDIR)/lib
PAHOINCLUDEDIR := $(PAHO_MQTT_C_USRDIR)/include
MQTTFLAGS := -I $(PAHOINCLUDEDIR) -L $(PAHOLIBDIR) -lpaho-mqtt3as -lpaho-mqtt3a
BLDFLAGS := $(CFLAGS) -O -I include/ -Wall -lpthread

SRCDIR := src
SOURCE_FILES_C_ECG2MQTT := readecg.c main.c sendmqtt.c
SOURCE_FILES_CPP_ECG2MQTT := 
OBJECT_FILES_ECG2MQTT = $(patsubst %.c,%.o,$(SOURCE_FILES_C_ECG2MQTT)) $(patsubst %.cpp,%.o,$(SOURCE_FILES_CPP_ECG2MQTT))

.PHONY := all clean fullclean paho run runsamplemqtt runecg2mqtt
all: samplemqtt

clean:
	rm -f samplemqtt ecg2mqtt *.o

fullclean: clean
	rm -rf $(PAHO_MQTT_C_REPODIR) $(PAHO_MQTT_C_BUILDDIR) $(PAHO_MQTT_C_USRDIR)

$(PAHO_MQTT_C_REPODIR):
	mkdir -p $(PAHO_MQTT_C_REPODIR)
	git clone --single-branch --branch $(PAHO_MQTT_C_BRANCH) $(PAHO_MQTT_C_URL) $(PAHO_MQTT_C_REPODIR)

$(PAHO_MQTT_C_BUILDDIR): $(PAHO_MQTT_C_REPODIR)
	$(MAKE) -C $(PAHO_MQTT_C_REPODIR) $(PAHO_MQTT_C_BLDFLAGS)

$(PAHO_MQTT_C_USRDIR): $(PAHO_MQTT_C_REPODIR) $(PAHO_MQTT_C_BUILDDIR)
	mkdir -p $(PAHO_MQTT_C_USRDIR)/lib $(PAHO_MQTT_C_USRDIR)/include $(PAHO_MQTT_C_USRDIR)/share/man/man1 $(PAHO_MQTT_C_USRDIR)/share/man/man2 $(PAHO_MQTT_C_USRDIR)/share/man/man3
	make -C $(PAHO_MQTT_C_REPODIR) install $(PAHO_MQTT_C_BLDFLAGS) DESTDIR=$(abspath $(PAHO_MQTT_C_USRDIR)) LDCONFIG=true prefix=

paho: $(PAHO_MQTT_C_USRDIR)

runsamplemqtt: samplemqtt
	LD_LIBRARY_PATH=$(PAHOLIBDIR) ./samplemqtt

runecg2mqtt: ecg2mqtt
	LD_LIBRARY_PATH=$(PAHOLIBDIR) ./ecg2mqtt -r 10 -H 147.46.244.130 -p 8883 -t hello -u demo -P guest --tls --cacert ./sp_cert_ca.crt

run: runecg2mqtt

samplemqtt: samplemqtt.o
	$(CC) -o $@ $< $(BLDFLAGS) $(MQTTFLAGS)

ecg2mqtt: $(OBJECT_FILES_ECG2MQTT)
	$(CC) -o $@ $^ $(BLDFLAGS) $(MQTTFLAGS)

%.o: $(SRCDIR)/%.c $(PAHO_MQTT_C_USRDIR)
	$(CC) -c $< $(BLDFLAGS) $(MQTTFLAGS)

%.o: $(SRCDIR)/%.cpp $(PAHO_MQTT_C_USRDIR)
	$(CXX) -c $< $(BLDFLAGS) $(CXXFLAGS) $(MQTTFLAGS)
