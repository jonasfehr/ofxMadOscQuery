#include "ofxMadOscQuery.h"
#include "OscQueryWebSocketClient.h"

#include <unordered_map>
#include <unordered_set>

namespace {
	const ofJson* resolveNodeByFullPath(const ofJson& root, std::string path) {
		if (path.empty()) return nullptr;
		if (path.front() == '/') path.erase(path.begin());
		const ofJson* node = &root;
		for (const auto& token : ofSplitString(path, "/", true, true)) {
			auto contentsIt = node->find("CONTENTS");
			if (contentsIt == node->end() || !contentsIt->is_object()) return nullptr;
			auto childIt = contentsIt->find(token);
			if (childIt == contentsIt->end()) return nullptr;
			node = &(*childIt);
		}
		return node;
	}

	const ofJson* findNodeByExactFullPath(const ofJson& node, const std::string& targetPath) {
		auto fullPathIt = node.find("FULL_PATH");
		if (fullPathIt != node.end() && fullPathIt->is_string() && fullPathIt->get<std::string>() == targetPath) {
			return &node;
		}

		auto contentsIt = node.find("CONTENTS");
		if (contentsIt == node.end() || !contentsIt->is_object()) return nullptr;
		for (auto it = contentsIt->begin(); it != contentsIt->end(); ++it) {
			if (const ofJson* found = findNodeByExactFullPath(it.value(), targetPath)) {
				return found;
			}
		}
		return nullptr;
	}

	std::string normalizeLookup(std::string value) {
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
			if (c == ' ' || c == '-') return '_';
			return static_cast<char>(std::tolower(c));
		});
		return value;
	}

	bool isMediaRootNode(const ofJson& node) {
		auto fullPathIt = node.find("FULL_PATH");
		if (fullPathIt == node.end() || !fullPathIt->is_string()) return false;
		const std::string fullPath = fullPathIt->get<std::string>();
		if (fullPath.rfind("/media/", 0) != 0) return false;
		if (fullPath.find('/', std::string("/media/").size()) != std::string::npos) return false;
		auto contentsIt = node.find("CONTENTS");
		return contentsIt != node.end() && contentsIt->is_object();
	}

	bool lookupMatches(const std::string& lhs, const std::string& rhs) {
		if (lhs.empty() || rhs.empty()) return false;
		if (lhs == rhs) return true;
		return lhs.find(rhs) != std::string::npos || rhs.find(lhs) != std::string::npos;
	}

	void collectStringLeaves(const ofJson& node, std::unordered_set<std::string>& outValues) {
		if (node.is_string()) {
			outValues.insert(normalizeLookup(node.get<std::string>()));
			return;
		}
		if (node.is_array()) {
			for (const auto& entry : node) collectStringLeaves(entry, outValues);
			return;
		}
		if (node.is_object()) {
			for (auto it = node.begin(); it != node.end(); ++it) collectStringLeaves(it.value(), outValues);
		}
	}

	std::unordered_set<std::string> normalizedSkipSet(const ofJson* skipKeys) {
		std::unordered_set<std::string> normalized;
		if (!skipKeys || !skipKeys->is_array()) return normalized;
		for (const auto& skipKey : *skipKeys) {
			if (skipKey.is_string()) {
				normalized.insert(normalizeLookup(skipKey.get<std::string>()));
			}
		}
		return normalized;
	}

	struct MediaCandidate {
		const ofJson* node = nullptr;
		std::string pageName;
	};

	std::string mediaPageNameFromNode(const std::string& fallbackName, const ofJson& mediaNode) {
		auto contentsIt = mediaNode.find("CONTENTS");
		if (contentsIt != mediaNode.end() && contentsIt->is_object()) {
			auto nameIt = contentsIt->find("name");
			if (nameIt != contentsIt->end() && nameIt->is_object()) {
				auto valueIt = nameIt->find("VALUE");
				if (valueIt != nameIt->end() && valueIt->is_array() && !valueIt->empty() && (*valueIt)[0].is_string()) {
					return (*valueIt)[0].get<std::string>();
				}
			}
		}
		return fallbackName;
	}

	MediaCandidate findMediaCandidateByNameHint(const ofJson& root, const std::string& mediaName) {
		MediaCandidate result;
		const std::string normalizedMediaName = normalizeLookup(mediaName);
		if (normalizedMediaName.empty()) return result;

		std::function<bool(const ofJson&, const std::string&)> visitMediaNode = [&](const ofJson& node, const std::string& fallbackKey) {
			if (isMediaRootNode(node)) {
				std::unordered_set<std::string> candidates;
				candidates.insert(normalizeLookup(fallbackKey));
				auto fullPathIt = node.find("FULL_PATH");
				if (fullPathIt != node.end() && fullPathIt->is_string()) {
					candidates.insert(normalizeLookup(fullPathIt->get<std::string>()));
				}
				auto descIt = node.find("DESCRIPTION");
				if (descIt != node.end() && descIt->is_string()) {
					candidates.insert(normalizeLookup(descIt->get<std::string>()));
				}
				collectStringLeaves(node, candidates);

				for (const auto& candidate : candidates) {
					if (!lookupMatches(candidate, normalizedMediaName)) continue;
					result.node = &node;
					result.pageName = mediaPageNameFromNode(fallbackKey, node);
					return true;
				}
			}

			auto contentsIt = node.find("CONTENTS");
			if (contentsIt == node.end() || !contentsIt->is_object()) return false;
			for (auto it = contentsIt->begin(); it != contentsIt->end(); ++it) {
				if (visitMediaNode(it.value(), it.key())) return true;
			}
			return false;
		};

		visitMediaNode(root, mediaName);
		return result;
	}

	bool populateMediaSubpageFromParameterMap(
		MadParameterPage& page,
		std::map<std::string, MadParameter>& parameterMap,
		const std::string& mediaName
	) {
		const std::string normalizedMediaName = normalizeLookup(mediaName);
		if (normalizedMediaName.empty()) return false;

		struct GroupMatch {
			int score = 0;
			std::vector<MadParameter*> parameters;
		};
		std::unordered_map<std::string, GroupMatch> groupedMatches;

		for (auto& entry : parameterMap) {
			const std::string& path = entry.second.getOscAddress();
			if (path.rfind("/media/", 0) != 0) continue;

			auto segments = ofSplitString(path, "/", true, true);
			if (segments.size() < 2) continue;
			const std::string groupKey = segments[1];
			const std::string normalizedGroupKey = normalizeLookup(groupKey);
			const std::string normalizedPath = normalizeLookup(path);
			const std::string normalizedParamName = normalizeLookup(entry.second.getName());

			int score = 0;
			if (lookupMatches(normalizedGroupKey, normalizedMediaName)) score += 120;
			if (lookupMatches(normalizedPath, normalizedMediaName)) score += 40;
			if (lookupMatches(normalizedParamName, normalizedMediaName)) score += 20;
			if (score == 0) continue;

			auto& group = groupedMatches[groupKey];
			group.score += score;
			group.parameters.push_back(&entry.second);
		}

		GroupMatch* bestMatch = nullptr;
		for (auto& groupedMatch : groupedMatches) {
			if (!bestMatch || groupedMatch.second.score > bestMatch->score ||
				(groupedMatch.second.score == bestMatch->score && groupedMatch.second.parameters.size() > bestMatch->parameters.size())) {
				bestMatch = &groupedMatch.second;
			}
		}

		if (!bestMatch) return false;
		for (auto* parameter : bestMatch->parameters) {
			page.addParameter(parameter);
		}
		return !page.isEmpty();
	}

	MediaCandidate findReferencedMediaCandidate(const ofJson& root, const ofJson& sourceNode) {
		MediaCandidate result;
		auto topIt = root.find("CONTENTS");
		if (topIt == root.end() || !topIt->is_object()) return result;
		auto mediaIt = topIt->find("media");
		if (mediaIt == topIt->end() || !mediaIt->is_object()) return result;
		auto mediaContentsIt = mediaIt->find("CONTENTS");
		if (mediaContentsIt == mediaIt->end() || !mediaContentsIt->is_object()) return result;

		std::unordered_set<std::string> sourceStrings;
		collectStringLeaves(sourceNode, sourceStrings);

		for (auto it = mediaContentsIt->begin(); it != mediaContentsIt->end(); ++it) {
			const auto& mediaNode = it.value();
			if (!mediaNode.is_object() || !isMediaRootNode(mediaNode)) continue;

			std::vector<std::string> candidates;
			candidates.push_back(normalizeLookup(it.key()));

			auto descIt = mediaNode.find("DESCRIPTION");
			if (descIt != mediaNode.end() && descIt->is_string()) {
				candidates.push_back(normalizeLookup(descIt->get<std::string>()));
			}

			std::string pageName = mediaPageNameFromNode(it.key(), mediaNode);
			candidates.push_back(normalizeLookup(pageName));

			for (const auto& candidate : candidates) {
				if (candidate.empty()) continue;
				if (sourceStrings.count(candidate) > 0) {
					result.node = &mediaNode;
					result.pageName = pageName;
					return result;
				}
			}
		}

		return result;
	}
}

ofxMadOscQuery::ofxMadOscQuery() { }
ofxMadOscQuery::~ofxMadOscQuery() {
	for (auto & parameter : parameterMap) {
		ofRemoveListener(parameter.second.oscSendEvent, this, &ofxMadOscQuery::oscSendToMadMapper);
	}
}

void ofxMadOscQuery::setup(string ip, int sendPort, int feedbackPort, int queryPort) {
	this->ip = ip;
	this->sendPort = sendPort;
	this->receivePort = feedbackPort;
	this->receiveAddress = "http://" + ip + ":" + ofToString(queryPort);
	oscSender.setup(ip, sendPort);
	oscReceiver.setup(feedbackPort);

	//    this->madMapperJson = receive(); // ofLoadJson("rawExample.json"); //
}

void ofxMadOscQuery::setup(string ip, int sendPort, int feedbackPort) {
	this->setup(ip, sendPort, feedbackPort, sendPort);
}
//--------------------------------------------------------------
ofJson ofxMadOscQuery::receive() {
	ofHttpResponse resp = ofLoadURL(receiveAddress);
	if (resp.data.size() == 0) {
		ofLogWarning("ofxMadOscQuery") << "MadMapper not open or OSCQuery endpoint unreachable: " << receiveAddress;
		return nullptr;
	}
	ofJson response;
	std::stringstream ssJSON;
	ssJSON << resp.data;
	ssJSON >> response;
	this->madMapperJson = response;
	return response;
}

void ofxMadOscQuery::updateValues() {
	this->madMapperJson = receive();
	for (auto & param : parameterMap) {
		string path = param.second.getOscAddress();
		vector<string> pathSeg = ofSplitString(path, "/");
		ofJson json = madMapperJson;
		for (int i = 1; i < (int)pathSeg.size(); i++) {
			json = json["CONTENTS"][pathSeg[i]];
		}
		if (!json.is_null()) {
			param.second.setFromRemoteRaw(json["VALUE"].at(0));
		}
	}
}
//--------------------------------------------------------------
// void ofxMadOscQuery::createParameterMap(ofJson json){
//
//
//
////    getParameterList(json["CONTENTS"]["surfaces"]["CONTENTS"], {"selected"});
//
////    getParameterList(json["CONTENTS"]["media"]["CONTENTS"], {"next", "per_type_selection", "previous", "select", "select_by_name", "selected"});
//
//    iterateContents(json["CONTENTS"]["surfaces"]);
//    cout << parameterMap.size() << endl;
//
//}

// void ofxMadOscQuery::iterateContents(ofJson json){
//     if(json["TYPE"]=="f") createParameter(json);
//     // dig deeper
//     for (nlohmann::json::iterator it = json.begin(); it != json.end(); ++it) {
//         if(it.key() == "CONTENTS"){
//             for (nlohmann::json::iterator it2 = it.value().begin(); it2 != it.value().end(); ++it2) {
//                 cout << it2.key() << endl;
//                 iterateContents(it2.value());
//             }
//             cout << "________________________" << endl;
//         }
//     }
// }

void ofxMadOscQuery::iterateFind(ofJson & jsonReturn, const ofJson & json, const string & key, const ofJson & jsonSkipKeys) {

	auto itFull = json.find("FULL_PATH");
	if (itFull == json.end() || !itFull->is_string()) return;
	string path = itFull->get<std::string>();
	vector<string> pathSeg = ofSplitString(path, "/");
	vector<string> keySeg = ofSplitString(key, "/");

	bool isKeyCompatible = true;
	for (size_t i = 0; i < keySeg.size() && i < pathSeg.size(); i++) {
		if (keySeg[i] == pathSeg[i]) {
		} else if (keySeg[i] == "*") {
			for (auto & skipKey : jsonSkipKeys) {
				if (pathSeg[i] == skipKey.get<std::string>()) {
					isKeyCompatible = false;
				}
			}
		} else {
			isKeyCompatible = false;
		}
	}

	auto itType = json.find("TYPE");
	if (itType != json.end() && itType->is_string() && (*itType == "f") && isKeyCompatible) {
		jsonReturn = json;
		return;
	}

	for (auto it = json.begin(); it != json.end(); ++it) {
		if (it.key() == "CONTENTS") {
			for (auto it2 = it.value().begin(); it2 != it.value().end(); ++it2) {
				iterateFind(jsonReturn, it2.value(), key, jsonSkipKeys);
			}
		}
	}
}

void ofxMadOscQuery::iterateFind(const ofJson & json, const string & key, MadParameterPage * customPage, const ofJson & jsonSkipKeys) {
	auto itFull = json.find("FULL_PATH");
	if (itFull == json.end() || !itFull->is_string()) return;
	string path = itFull->get<std::string>();
	vector<string> pathSeg = ofSplitString(path, "/");
	vector<string> keySeg = ofSplitString(key, "/");

	bool isKeyCompatible = true;
	int j = 0;
	int i = 0;
	while (j < (int)keySeg.size() && i < (int)pathSeg.size()) {
		if (keySeg[j] == pathSeg[i]) {
			j++;
			i++;
		} else if (keySeg[j] == "*") {
			for (auto & skipKey : jsonSkipKeys) {
				for (int n = j; n <= i; n++) {
					if (pathSeg[n] == skipKey.get<std::string>()) {
						isKeyCompatible = false;
					}
				}
			}

			if ((int)keySeg.size() - 1 != j) {
				std::size_t foundPos = path.find("/" + keySeg[j + 1]);
				if (foundPos != std::string::npos) {
					int endPos = (int)foundPos + (int)keySeg[j + 1].size() + 1;
					if ((endPos == (int)path.size()) || path[endPos] == '/') {
						while (i < (int)pathSeg.size() && pathSeg[i] != keySeg[j + 1]) {
							i++;
						}
						if (i == j) isKeyCompatible = false;
					} else {
						isKeyCompatible = false;
					}
				}
			} else {
				if ((int)pathSeg.size() == (int)keySeg.size() && isKeyCompatible) {
					j = (int)keySeg.size();
				}
			}
			j++;
		} else {
			isKeyCompatible = false;
			break;
		}
	}

	auto itType = json.find("TYPE");
	if (itType != json.end() && itType->is_string() && (*itType == "f") && isKeyCompatible) {
		(*customPage).addParameter(createParameter(json));
	}

	for (auto it = json.begin(); it != json.end(); ++it) {
		if (it.key() == "CONTENTS") {
			for (auto it2 = it.value().begin(); it2 != it.value().end(); ++it2) {
				iterateFind(it2.value(), key, customPage, jsonSkipKeys);
			}
		}
	}
}

map<string, ofJson> ofxMadOscQuery::getContentMap(const ofJson & json, const string & key, const vector<string> & skipKeys) {
	map<string, ofJson> contentMap;
	vector<string> keySeg = ofSplitString(key, "/");
	if (keySeg.size() < 2) return contentMap;

	auto itTop = json.find("CONTENTS");
	if (itTop == json.end() || !itTop->is_object()) return contentMap;

	auto itCat = itTop->find(keySeg[1]);
	if (itCat == itTop->end() || !itCat->is_object()) return contentMap;

	auto itInner = itCat->find("CONTENTS");
	if (itInner == itCat->end() || !itInner->is_object()) return contentMap;

	const auto & container = *itInner;
	for (auto it = container.begin(); it != container.end(); ++it) {
		bool isKeyCompatible = true;
		if (keySeg.size() > 2 && keySeg[2] == "*") {
			for (auto & skipKey : skipKeys) {
				if (it.key() == skipKey) {
					isKeyCompatible = false;
				}
			}
		} else {
			isKeyCompatible = false;
		}

		if (isKeyCompatible) {
			contentMap[it.key()] = it.value();
		}
	}

	return contentMap;
}

void ofxMadOscQuery::getConnectedMediaName(string * mediaName, const ofJson & json, const string & key, const ofJson & jsonSkipKeys) {
	auto itFull = json.find("FULL_PATH");
	if (itFull == json.end() || !itFull->is_string()) return;
	string path = itFull->get<std::string>();
	vector<string> pathSeg = ofSplitString(path, "/");
	vector<string> keySeg = ofSplitString(key, "/");

	bool isKeyCompatible = true;
	int j = 0;
	int i = 0;
	while (j < (int)keySeg.size() && i < (int)pathSeg.size()) {
		if (keySeg[j] == pathSeg[i]) {
			j++;
			i++;
		} else if (keySeg[j] == "*") {
			if ((int)keySeg.size() - 1 != j) {
				std::size_t foundPos = path.find("/" + keySeg[j + 1]);
				if (foundPos != std::string::npos) {
					int endPos = (int)foundPos + (int)keySeg[j + 1].size() + 1;
					if ((endPos == (int)path.size()) || path[endPos] == '/') {
						while (i < (int)pathSeg.size() && pathSeg[i] != keySeg[j + 1]) {
							i++;
						}
					} else {
						isKeyCompatible = false;
					}
				}
			} else {
				if ((int)pathSeg.size() == (int)keySeg.size() && isKeyCompatible) {
					j = (int)keySeg.size();
				}
			}

			for (auto & skipKey : jsonSkipKeys) {
				for (int n = j; n < i; n++) {
					if (pathSeg[n] == skipKey.get<std::string>()) {
						isKeyCompatible = false;
					}
				}
			}
			j++;
		} else {
			isKeyCompatible = false;
			j++;
			i++;
		}
	}
	auto itType = json.find("TYPE");
	if (itType != json.end() && itType->is_string() && (*itType == "s") && isKeyCompatible) {
		auto itVal = json.find("VALUE");
		if (itVal != json.end() && itVal->is_array() && !itVal->empty() && (*itVal)[0].is_string()) {
			*mediaName = (*itVal)[0].get<std::string>();
		}
	}

	for (auto it = json.begin(); it != json.end(); ++it) {
		if (it.key() == "CONTENTS") {
			for (auto it2 = it.value().begin(); it2 != it.value().end(); ++it2) {
				getConnectedMediaName(mediaName, it2.value(), key, jsonSkipKeys);
			}
		}
	}
}

void ofxMadOscQuery::createCustomPages(ofxMidiDevice * midiDevice, const ofJson & jsonPages, const ofJson & madMapperJson) {
	for (auto & page : jsonPages["pages"]) {
		std::string name = page["name"];
		MadParameterPage customPage = MadParameterPage(name, midiDevice);
		for (auto & element : page["elements"]) {
			iterateFind(madMapperJson, element, &customPage, page["skipKeys"]);
		}
		pages.push_back(customPage);
	}

	ofJson jsonSubpages = ofLoadJson("subpages.json");
	const ofJson* mediaSkipKeys = nullptr;
	static bool warnedMissingMediaSkipKeysInCustomPages = false;
	if (jsonSubpages.contains("media") && jsonSubpages["media"].contains("skipKeys") && jsonSubpages["media"]["skipKeys"].is_array()) {
		mediaSkipKeys = &jsonSubpages["media"]["skipKeys"];
	} else if (!warnedMissingMediaSkipKeysInCustomPages) {
		ofLogWarning("ofxMadOscQuery") << "subpages.json: missing or invalid media.skipKeys array; media filtering will not skip configured keys";
		warnedMissingMediaSkipKeysInCustomPages = true;
	}
	bool hasRawExampleJson = false;
	ofJson rawExampleJson;
	if (ofFile::doesFileExist("rawExample.json")) {
		rawExampleJson = ofLoadJson("rawExample.json");
		hasRawExampleJson = !rawExampleJson.is_null();
	}

	for (auto & param : parameterMap) {
		if (param.second.isMaster) {
			std::string name = param.second.parentName;
			std::string parentPath = param.second.getOscAddress();
			auto slash = parentPath.find_last_of('/');
			if (slash != std::string::npos) parentPath = parentPath.substr(0, slash);

			MadParameterPage customSubpage = MadParameterPage(name, midiDevice, 13, true);
			for (auto & element : jsonSubpages["opacity"]["elements"]) {
				string newKey = "*/" + name + element.get<std::string>();
				iterateFind(madMapperJson, newKey, &customSubpage, jsonSubpages["opacity"]["skipKeys"]);
			}
			subPages.push_back(customSubpage);

			for (auto & element : jsonSubpages["luminosity"]["elements"]) {
				string newKey = "*/" + name + element.get<std::string>();
				iterateFind(madMapperJson, newKey, &customSubpage, jsonSubpages["luminosity"]["skipKeys"]);
			}
			subPages.push_back(customSubpage);

			string newKey = "*/" + name + "/visual/name";
			string mediaName;
			getConnectedMediaName(&mediaName, madMapperJson, newKey, jsonSubpages["media"]["skipKeys"]);

			if (mediaName.empty()) {
				if (const ofJson* sourceNode = resolveNodeByFullPath(madMapperJson, parentPath)) {
					auto mediaCandidate = findReferencedMediaCandidate(madMapperJson, *sourceNode);
					if (mediaCandidate.node && !mediaCandidate.pageName.empty()) {
						mediaName = mediaCandidate.pageName;
					}
					if (mediaName.empty() && mediaCandidate.node) {
						mediaName = mediaPageNameFromNode(name, *mediaCandidate.node);
					}
				}
			}

			if (mediaName.empty() && hasRawExampleJson) {
				if (const ofJson* sourceNode = resolveNodeByFullPath(rawExampleJson, parentPath)) {
					auto mediaCandidate = findReferencedMediaCandidate(rawExampleJson, *sourceNode);
					if (mediaCandidate.node && !mediaCandidate.pageName.empty()) {
						mediaName = mediaCandidate.pageName;
					}
					if (mediaName.empty() && mediaCandidate.node) {
						mediaName = mediaPageNameFromNode(name, *mediaCandidate.node);
					}
				}
			}

			if (!mediaName.empty() && mediaName != "4x4.png") {
				const ofJson* mediaNode = nullptr;
				std::string mediaPageName = mediaName;
				bool mediaPageBuiltFromParameters = false;
				bool mediaNodeFromRawExample = false;
				std::vector<std::string> mediaNameVariants;
				mediaNameVariants.push_back(mediaName);
				std::string normalizedMediaName = mediaName;
				ofStringReplace(normalizedMediaName, " ", "_");
				if (normalizedMediaName != mediaName) mediaNameVariants.push_back(normalizedMediaName);

				for (const auto& mediaNameVariant : mediaNameVariants) {
					mediaNode = resolveNodeByFullPath(madMapperJson, "/media/" + mediaNameVariant);
					if (!mediaNode) mediaNode = findNodeByExactFullPath(madMapperJson, "/media/" + mediaNameVariant);
					if (mediaNode && !isMediaRootNode(*mediaNode)) mediaNode = nullptr;
					if (!mediaNode) continue;
					mediaPageName = mediaPageNameFromNode(mediaPageName, *mediaNode);
					break;
				}

				if (!mediaNode) {
					auto mediaCandidate = findMediaCandidateByNameHint(madMapperJson, mediaName);
					if (mediaCandidate.node) {
						mediaNode = mediaCandidate.node;
						if (!mediaCandidate.pageName.empty()) mediaPageName = mediaCandidate.pageName;
					}
				}

				if (!mediaNode) {
					if (const ofJson* sourceNode = resolveNodeByFullPath(madMapperJson, parentPath)) {
						auto mediaCandidate = findReferencedMediaCandidate(madMapperJson, *sourceNode);
						if (mediaCandidate.node) {
							mediaNode = mediaCandidate.node;
							if (!mediaCandidate.pageName.empty()) mediaPageName = mediaCandidate.pageName;
						}
					}
				}

				if (!mediaNode && hasRawExampleJson) {
					for (const auto& mediaNameVariant : mediaNameVariants) {
						mediaNode = resolveNodeByFullPath(rawExampleJson, "/media/" + mediaNameVariant);
						if (!mediaNode) mediaNode = findNodeByExactFullPath(rawExampleJson, "/media/" + mediaNameVariant);
						if (mediaNode && !isMediaRootNode(*mediaNode)) mediaNode = nullptr;
						if (!mediaNode) continue;
						mediaPageName = mediaPageNameFromNode(mediaPageName, *mediaNode);
						mediaNodeFromRawExample = true;
						break;
					}

					if (!mediaNode) {
						auto mediaCandidate = findMediaCandidateByNameHint(rawExampleJson, mediaName);
						if (mediaCandidate.node) {
							mediaNode = mediaCandidate.node;
							if (!mediaCandidate.pageName.empty()) mediaPageName = mediaCandidate.pageName;
							mediaNodeFromRawExample = true;
						}
					}
				}

				MadParameterPage customMediaSubpage = MadParameterPage(mediaPageName, midiDevice, 13, true);
				if (mediaNode) {
					auto chosenPathIt = mediaNode->find("FULL_PATH");
					if (chosenPathIt != mediaNode->end() && chosenPathIt->is_string()) {
						ofLogNotice("ofxMadOscQuery") << "Media node selected for '" << mediaName
							<< "': " << chosenPathIt->get<std::string>();
					}
					setupPageFromJson(subPages, customMediaSubpage, midiDevice, *mediaNode, "media", mediaSkipKeys);
				} else {
					mediaPageBuiltFromParameters = populateMediaSubpageFromParameterMap(customMediaSubpage, parameterMap, mediaName);
				}

				if (!customMediaSubpage.isEmpty()) {
					ofLogNotice("ofxMadOscQuery") << "Created media subpage '" << mediaPageName
						<< "' with " << customMediaSubpage.getParameters()->size() << " parameters"
						<< " (from parameter map=" << mediaPageBuiltFromParameters
						<< ", rawExample=" << mediaNodeFromRawExample << ")";
					param.second.setConnectedMediaName(mediaPageName);
					subPages.push_back(customMediaSubpage);
				} else {
					ofLogNotice("ofxMadOscQuery") << "Media subpage for '" << mediaName << "' stayed empty"
						<< " (fallback media node found=" << (mediaNode != nullptr)
						<< ", parameter map fallback=" << mediaPageBuiltFromParameters
						<< ", rawExample=" << mediaNodeFromRawExample << ")";
				}
			}
		}
	}
}

void ofxMadOscQuery::createCustomPage(std::list<MadParameterPage> & pages, ofxMidiDevice * midiDevice, const ofJson & json) {
	for (auto & page : json["pages"]) {
		std::string name = page["name"];
		MadParameterPage customPage = MadParameterPage(name, midiDevice);
		for (auto & element : page["surfaces"]) {
			addParameterToCustomPage(element, "surfaces", &customPage);
		}
		for (auto & element : page["fixtures"]) {
			addParameterToCustomPage(element, "fixtures", &customPage);
		}
		for (auto & element : page["media"]) {
			addParameterToCustomPage(element, "media", &customPage);
		}
		for (auto & element : page["modules"]) {
			addParameterToCustomPage(element, "modules", &customPage);
		}
		pages.push_front(customPage);
	}
}

void ofxMadOscQuery::addParameterToCustomPage(const ofJson & element, const std::string & type, MadParameterPage * customPage) {
	std::string elementName;
	if (element.is_string())
		elementName = element.get<std::string>();
	else
		elementName = ofToString(element).substr(1, ofToString(element).size() - 2);
	std::string typeName = "/" + type + "/";

	for (auto & surfaceParam : parameterMap) {
		std::string paramName = ofToString(surfaceParam.first);
		if (elementName == "*") {
			if ((surfaceParam.first.rfind(typeName, 0) == 0) && !(surfaceParam.first.rfind(typeName + "selected", 0) == 0)) {
				(*customPage).addParameter(&surfaceParam.second);
			}
		} else if (elementName == paramName) {
			(*customPage).addParameter(&surfaceParam.second);
		} else if (matchesGroupWildcard(paramName, elementName)) {
			(*customPage).addParameter(&surfaceParam.second);
		} else {
			auto parsedName = elementName.substr(2, elementName.size() - 2);
			std::vector<std::string> seglist;
			std::stringstream ss(paramName);
			std::string segment;
			while (std::getline(ss, segment, '/')) {
				seglist.push_back(segment);
			}
			if (seglist.size() > 3 && seglist[3] == parsedName && seglist[2] != "selected") {
				(*customPage).addParameter(&surfaceParam.second);
			}
		}
	}
}

bool ofxMadOscQuery::matchesGroupWildcard(const std::string & paramNameIn, const std::string & elementName) {
	std::string paramName = paramNameIn;
	if (paramName.find("Group") == std::string::npos) {
		return false;
	}
	paramName = paramName.substr(1, ofToString(paramName).size());
	std::stringstream ss(paramName);
	std::string segment;
	std::vector<std::string> segList;
	while (std::getline(ss, segment, '/')) {
		segList.push_back(segment);
	}

	for (int i = (int)segList.size() - 1; i > 1; i--) {
		auto seg = segList[i];
		if (!seg.empty() && seg != "Group" && elementName.find(seg) != std::string::npos) {
			return true;
		}
	}
	return false;
}

void ofxMadOscQuery::createSubPages(std::list<MadParameterPage> & pages, ofxMidiDevice * midiDevice, const ofJson & json) {
	auto itTop = json.find("CONTENTS");
	if (itTop == json.end() || !itTop->is_object()) return;

	ofJson jsonSubpages = ofLoadJson("subpages.json");
	const ofJson* mediaSkipKeys = nullptr;
	static bool warnedMissingMediaSkipKeysInCreateSubPages = false;
	if (jsonSubpages.contains("media") && jsonSubpages["media"].contains("skipKeys") && jsonSubpages["media"]["skipKeys"].is_array()) {
		mediaSkipKeys = &jsonSubpages["media"]["skipKeys"];
	} else if (!warnedMissingMediaSkipKeysInCreateSubPages) {
		ofLogWarning("ofxMadOscQuery") << "subpages.json: missing or invalid media.skipKeys array; media filtering will not skip configured keys";
		warnedMissingMediaSkipKeysInCreateSubPages = true;
	}

	const std::initializer_list<std::string> keyTypes{ "surfaces", "media", "fixtures" };
	for (auto & keyType : keyTypes) {
		auto itType = itTop->find(keyType);
		if (itType == itTop->end() || !itType->is_object()) continue;
		auto itCont = itType->find("CONTENTS");
		if (itCont == itType->end() || !itCont->is_object()) continue;

		for (auto it = itCont->begin(); it != itCont->end(); ++it) {
			const auto & element = it.value();
			auto itDesc = element.find("DESCRIPTION");
			if (itDesc == element.end() || !itDesc->is_string()) continue;

			auto skipDescriptions = { "Next", "Per Type Selection", "Previous", "Select", "Select By Name", "Selected", "selected", "children" };
			bool shouldSkip = false;
			for (auto & skipDescription : skipDescriptions) {
				if (*itDesc == skipDescription) {
					shouldSkip = true;
					break;
				}
			}
			if (shouldSkip) continue;

			const std::string currentKeyType = keyType;
			auto keyword = itDesc->get<std::string>();
			MadParameterPage page = MadParameterPage(keyword, midiDevice, true);
			const ofJson* pageSkipKeys = (currentKeyType == "media") ? mediaSkipKeys : nullptr;
			setupPageFromJson(pages, page, midiDevice, element, currentKeyType, pageSkipKeys);
			if (!page.isEmpty()) {
				pages.push_back(page);
			}
		}
	}
}

void ofxMadOscQuery::setupPageFromJson(std::list<MadParameterPage> & pages, MadParameterPage & page, ofxMidiDevice * midiDevice, const ofJson & element, const string & keyType, const ofJson * skipKeys) {
	auto itCont = element.find("CONTENTS");
	if (itCont == element.end() || !itCont->is_object()) return;
	const auto skipSet = normalizedSkipSet(skipKeys);

	for (auto it = itCont->begin(); it != itCont->end(); ++it) {
		const auto & contents = it.value();
		const std::string normalizedEntryKey = normalizeLookup(it.key());
		auto itDesc = contents.find("DESCRIPTION");
		std::string description;
		if (itDesc != contents.end() && itDesc->is_string()) {
			description = itDesc->get<std::string>();
		}
		const std::string normalizedDescription = normalizeLookup(description);

		auto skipDescriptions = { "Resolution", "Assign To Selected Surfaces", "Assign To All Surfaces", "Restart", "Select", "selected" };
		for (auto & skipDescription : skipDescriptions) {
			auto innerDesc = element["CONTENTS"].find("DESCRIPTION");
			if (innerDesc != element["CONTENTS"].end() && *innerDesc == skipDescription) {
				return;
			}
		}

		auto ctype = contents.find("TYPE");
		if (keyType == "media") {
			if (!skipSet.empty() && (skipSet.count(normalizedDescription) > 0 || skipSet.count(normalizedEntryKey) > 0)) {
				continue;
			}
			if (ctype != contents.end() && ctype->is_string() && (*ctype == "f" || *ctype == "i")) {
				page.addParameter(createParameter(contents));
				continue;
			}
			if (contents.find("CONTENTS") != contents.end() && contents["CONTENTS"].is_object()) {
				setupPageFromJson(pages, page, midiDevice, contents, keyType, skipKeys);
			}
			continue;
		}

		if (description.empty()) continue;

		if (description == "Opacity") {
			MadParameter * newOpacityParameter = createParameter(contents);
			page.addParameter(newOpacityParameter);
			bool bIsGroup = false;
			if (keyType == "surfaces" && element["CONTENTS"].find("output") == element["CONTENTS"].end()) {
				bIsGroup = true;
			} else if (keyType == "fixtures" && element["CONTENTS"].size() > 10) {
				bIsGroup = true;
			} else
				return;
			newOpacityParameter->setIsGroup(bIsGroup);
			auto groupNameIt = element.find("DESCRIPTION");
			std::string groupName = groupNameIt != element.end() && groupNameIt->is_string() ? groupNameIt->get<std::string>() : "Group";
			MadParameterPage subPage = MadParameterPage(groupName, midiDevice, true);
			auto subContIt = contents.find("CONTENTS");
			if (subContIt != contents.end() && subContIt->is_object()) {
				setupPageFromJson(pages, subPage, midiDevice, *subContIt, keyType, skipKeys);
			}
			string searchString = groupName + "/*/opacity";
			auto customJson = ofJson::parse("{ \"pages\": [{\"name\": \"" + groupName + "_SubPage\", \"surfaces\": [\"" + searchString + "\"]}]}");
			createCustomPage(pages, midiDevice, customJson);
		} else if (description == "Color") {
			auto colorCont = contents.find("CONTENTS");
			if (colorCont != contents.end() && colorCont->is_object()) {
				for (auto itColor = colorCont->begin(); itColor != colorCont->end(); ++itColor) {
					auto cdesc = itColor.value().find("DESCRIPTION");
					if (cdesc != itColor.value().end() && cdesc->is_string()) {
						auto s = cdesc->get<std::string>();
						if (s == "Red" || s == "Green" || s == "Blue") {
							page.addParameter(createParameter(itColor.value()));
						}
					}
				}
			}
		} else if (description == "fx") {
			auto fxCont = contents.find("CONTENTS");
			if (fxCont != contents.end() && fxCont->is_object()) {
				for (auto itFx = fxCont->begin(); itFx != fxCont->end(); ++itFx) {
					auto fdesc = itFx.value().find("DESCRIPTION");
					auto ftype = itFx.value().find("TYPE");
					if (fdesc != itFx.value().end() && ftype != itFx.value().end() && fdesc->is_string() && ftype->is_string() && *fdesc != "FX Type" && *ftype == "f") {
						page.addParameter(createParameter(itFx.value()));
					}
				}
			}
		}
	}
}

void ofxMadOscQuery::oscSendToMadMapper(ofxOscMessage & m) {
	oscSender.sendMessage(m, false);
}

void ofxMadOscQuery::oscReceiveMessages(ofParameterGroup & syncGroup) {
	while (oscReceiver.hasWaitingMessages()) {
		oscReceiver.getParameter(syncGroup);
	}
	//
	//    while(oscReceiver.hasWaitingMessages()){
	//        ofxOscMessage m;
	//        oscReceiver.getNextMessage(m);
	//        ofLog() << "Received message on adress: " << m.getAddress() << endl;
	//
	//        if(m.getAddress() == "/media/select_by_name"){
	//            lastSelectedMedia = m.getArgAsString(0);
	//            ofLog() << "Connected Media " << lastSelectedMedia << endl;
	//
	//            ofNotifyEvent(mediaNameE, lastSelectedMedia, this);
	//        }
	//    }
}

//--------------------------------------------------------------
MadParameter * ofxMadOscQuery::createParameter(ofJson parameterValues) {
	std::string key = parameterValues["FULL_PATH"];
	parameterMap[key] = MadParameter(parameterValues);
	auto val = &parameterMap.operator[](key);
	ofAddListener(val->oscSendEvent, this, &ofxMadOscQuery::oscSendToMadMapper);
	return val;
}

// -------------------------------------------------------------- WebSocket support
bool ofxMadOscQuery::connectWebSocket(int port) {
	if (!wsClient) {
		wsClient = std::make_unique<OscQueryWebSocketClient>();
	}
	std::string host = ip.empty() ? "127.0.0.1" : ip;
	wsConnected = wsClient->connect(host, port, [this](const std::string & msg) { handleWebSocketMessage(msg); });
	if (!wsConnected) {
		ofLogWarning("ofxMadOscQuery") << "WebSocket connect failed to " << host << ":" << port;
		subscribedPaths.clear();
	} else {
		ofLogNotice("ofxMadOscQuery") << "WebSocket connected on port " << port;
	}
	return wsConnected;
}

void ofxMadOscQuery::disconnectWebSocket() {
	if (wsClient) wsClient->disconnect();
	wsConnected = false;
	subscribedPaths.clear();
}

bool ofxMadOscQuery::isWebSocketConnected() const { return wsConnected; }

void ofxMadOscQuery::subscribeParameter(const std::string & path) {
	if (!wsClient || !wsConnected) return;
	if (subscribedPaths.find(path) != subscribedPaths.end()) return;
	ofJson msg;
	msg["COMMAND"] = "LISTEN";
	msg["DATA"] = path;
	if (wsClient->sendText(msg.dump())) {
		subscribedPaths.insert(path);
	}
}

void ofxMadOscQuery::subscribeAllParameters() {
	if (!wsClient || !wsConnected) return;
	for (auto & kv : parameterMap) subscribeParameter(kv.first);
}

void ofxMadOscQuery::subscribePageParameters(const MadParameterPage & page) {
	if (!wsClient || !wsConnected) return;
	auto * params = page.getParameters();
	if (!params) return;
	for (auto * p : *params) {
		if (!p) continue;
		subscribeParameter(p->getOscAddress());
	}
}

void ofxMadOscQuery::unsubscribeAll() {
	if (wsClient && wsConnected) {
		ofJson msg;
		msg["COMMAND"] = "LISTEN";
		msg["DATA"] = ofJson::array();
		wsClient->sendText(msg.dump());
	}
	subscribedPaths.clear();
}

void ofxMadOscQuery::pullPageValues(const MadParameterPage & page) {
	madMapperJson = receive();
	auto * params = page.getParameters();
	if (!params) return;
	for (auto * p : *params) {
		if (!p) continue;
		const std::string path = p->getOscAddress();
		if (path.empty()) continue;
		vector<string> seg = ofSplitString(path, "/");
		ofJson json = madMapperJson;
		bool found = true;
		for (size_t i = 1; i < seg.size(); ++i) {
			auto it = json.find("CONTENTS");
			if (it == json.end()) { found = false; break; }
			json = (*it)[seg[i]];
			if (json.is_null()) { found = false; break; }
		}
		if (found && json.contains("VALUE") && json["VALUE"].is_array() && !json["VALUE"].empty()) {
			std::lock_guard<std::mutex> lock(paramMutex);
			p->setFromRemoteRaw(json["VALUE"].at(0));
		}
	}
}

void ofxMadOscQuery::handleWebSocketMessage(const std::string & msg) {
	ofJson json;
	try {
		json = ofJson::parse(msg);
	} catch (const std::exception & e) {
		ofLogWarning("ofxMadOscQuery") << "WS parse failed: " << e.what();
		return;
	}

	std::string path;
	if (json.contains("PATH") && json["PATH"].is_string()) path = json["PATH"].get<std::string>();
	else if (json.contains("NAME") && json["NAME"].is_string()) path = json["NAME"].get<std::string>();
	else if (json.contains("FULL_PATH") && json["FULL_PATH"].is_string()) path = json["FULL_PATH"].get<std::string>();

	if (path.empty()) {
		ofLogWarning("ofxMadOscQuery") << "WS message missing PATH/NAME/FULL_PATH";
		return;
	}

	// Forward raw path updates so higher layers (e.g. cue timeline grid) can react
	// even when the path is not part of parameterMap.
	ofNotifyEvent(webSocketPathE, path, this);

	std::string lookupPath = path;
	if (lookupPath.rfind("/medias/", 0) == 0) {
		lookupPath = "/media/" + lookupPath.substr(std::string("/medias/").size());
	}

	auto it = parameterMap.find(lookupPath);
	if (it == parameterMap.end()) {
		return;
	}

	float value = 0.f;
	bool gotVal = false;
	if (json.contains("VALUE") && json["VALUE"].is_array() && !json["VALUE"].empty()) {
		value = json["VALUE"].at(0).get<float>();
		gotVal = true;
	} else if (json.contains("ARGS") && json["ARGS"].is_array() && !json["ARGS"].empty()) {
		value = json["ARGS"].at(0).get<float>();
		gotVal = true;
	}

	if (!gotVal) {
		ofLogWarning("ofxMadOscQuery") << "WS message missing VALUE/ARGS for " << lookupPath;
		return;
	}

	{
		std::lock_guard<std::mutex> lock(paramMutex);
		float current = it->second.get();
		if (std::fabs(current - value) < 1e-6f) return; // avoid echo loop
		it->second.setFromRemoteRaw(value);
	}
}

