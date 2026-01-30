ADDON_NAME = ofxMadOscQuery
ADDON_DESCRIPTION = OSCQuery helper with optional WebSocket feedback
ADDON_AUTHOR = Jonas Fehr
ADDON_TAGS = osc oscquery
ADDON_URL = https://github.com/jonasfehr/ofxMadOscQuery

# Core includes
ADDON_INCLUDES += src

# Poco headers
ADDON_INCLUDES += $(OF_ROOT)/libs/poco/include

# Poco libs (macOS). Adjust for other platforms if needed.
ADDON_LDFLAGS += -L$(OF_ROOT)/libs/poco/lib/osx
ADDON_LDFLAGS += -lPocoFoundation -lPocoNet -lPocoUtil -lPocoXML -lPocoJSON -lPocoNetSSL

# If you target other platforms, add their library paths here, e.g.:
# ADDON_LDFLAGS += -L$(OF_ROOT)/libs/poco/lib/linux64
# ADDON_LDFLAGS += -L$(OF_ROOT)/libs/poco/lib/vs

# No extra compiler flags required by default
# ADDON_CFLAGS +=
# ADDON_CPPFLAGS +=
# ADDON_CXXFLAGS +=
# ADDON_LDFLAGS +=
