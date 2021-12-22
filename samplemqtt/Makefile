
PAHO_MQTT_C_URL := https://github.com/eclipse/paho.mqtt.c
PAHO_MQTT_C_BRANCH := v1.3.9
PAHO_MQTT_C_REPODIR := paho.mqtt.c
PAHO_MQTT_C_BUILDDIR := paho.mqtt.c.build
PAHO_MQTT_C_USRDIR := paho.mqtt.c.usr
PAHO_MQTT_C_BLDFLAGS = blddir=$(abspath $(PAHO_MQTT_C_BUILDDIR))

PAHOLIBDIR := $(PAHO_MQTT_C_USRDIR)/lib
PAHOINCLUDEDIR := $(PAHO_MQTT_C_USRDIR)/include
MQTTFLAGS := -I $(PAHOINCLUDEDIR) -L $(PAHOLIBDIR) -lpaho-mqtt3as -lpaho-mqtt3a


.PHONY := all
all: samplemqtt

clean:
	rm -f samplemqtt

$(PAHO_MQTT_C_REPODIR):
	mkdir -p $(PAHO_MQTT_C_REPODIR)
	git clone --single-branch --branch $(PAHO_MQTT_C_BRANCH) $(PAHO_MQTT_C_URL) $(PAHO_MQTT_C_REPODIR)


run: samplemqtt
	./samplemqtt

%: %.c
	$(CC) $(CFLAGS) -o $@ $<
