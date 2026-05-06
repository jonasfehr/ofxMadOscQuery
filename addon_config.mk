ADDON_NAME = ofxMadOscQuery
ADDON_DESCRIPTION = OSCQuery helper with optional WebSocket feedback
ADDON_AUTHOR = Jonas Fehr
ADDON_TAGS = osc oscquery
ADDON_URL = https://github.com/jonasfehr/ofxMadOscQuery

common:
	# Core includes
	ADDON_INCLUDES += src

	# Poco headers and libs
	ADDON_INCLUDES += $(OF_ROOT)/libs/poco/include
	ADDON_LDFLAGS += -L$(OF_ROOT)/libs/poco/lib/osx
	ADDON_LDFLAGS += -lPocoFoundation -lPocoNet -lPocoUtil -lPocoXML -lPocoJSON -lPocoNetSSL
