#include "MadParameterPage.hpp"

#include "MadParameter.h"
#include "ofxMidiDevice.h"

namespace {
std::string controlLabelForSlot(ofxMidiDevice* midiDevice, int slot) {
	if (!midiDevice || slot <= 0) return std::string();
	const std::string slotStr = ofToString(slot);
	const std::array<std::string, 3> roles{
		"param." + slotStr + ".val_ctrl",
		"param." + slotStr + ".fader",
		"param." + slotStr + ".knob",
	};
	for (const auto& role : roles) {
		auto it = midiDevice->bindings.find(role);
		if (it != midiDevice->bindings.end() && midiDevice->midiComponents.count(it->second)) {
			return it->second;
		}
	}

	const std::array<std::string, 2> fallbacks{
		"fader_" + slotStr,
		"knob_" + slotStr,
	};
	for (const auto& label : fallbacks) {
		if (midiDevice->midiComponents.count(label)) return label;
	}
	return std::string();
}
}

MadParameterPage::MadParameterPage(std::string name, ofxMidiDevice *midiDevice, int numParamVis, bool isSubpage, bool isGroup)
	: bSubpage(isSubpage)
	, bIsGroup(isGroup)
	, numParamVis(numParamVis)
	, name(std::move(name))
	, midiDevice(midiDevice)
{
	linkedParamGroup.setName("Page");
}

MadParameterPage::~MadParameterPage() = default;

void MadParameterPage::setValuesOnDevice(ofAbstractParameter &p)
{
	auto parameter = parameters.begin();
	for (int i = 1; i < range.first; i++)
	{
		parameter++;
	}

	for (int i = 1; i < numParamVis + 1; i++)
	{
		const std::string controlLabel = controlLabelForSlot(midiDevice, i);
		if (parameter != parameters.end())
		{
			if ((*parameter)->updateFromMidi) {
				parameter++;
				continue;
			}
			if (!controlLabel.empty()) {
				auto& control = midiDevice->midiComponents[controlLabel];
				float target = (*parameter)->get();
				if (std::fabs(control.value.get() - target) > 1e-6f) {
					control.value.disableEvents();
					control.value.set(target);
					control.value.enableEvents();
					control.update();
				}
			}

			parameter++;
		}
		else
		{
			if (!controlLabel.empty()) {
				auto& control = midiDevice->midiComponents[controlLabel];
				if (std::fabs(control.value.get()) > 1e-6f) {
					control.value.disableEvents();
					control.value.set(0.f);
					control.value.enableEvents();
					control.update();
				}
			}
		}
	}
}

void MadParameterPage::addParameter(MadParameter *parameter)
{
	std::string paramName = parameter->getParameterName();
	if (this->name != "opacity")
	{
		if (paramName == "opacity" || paramName == "red" || paramName == "green" || paramName == "blue")
		{
			parameters.push_front(parameter);
		}
		else
		{
			parameters.push_back(parameter);
		}
	}
	int upper = parameters.size();
	if (upper > numParamVis)
		upper = numParamVis;
	range = std::make_pair(1, upper);
}

void MadParameterPage::setLowerBound(int lower)
{
	int upper = lower + numParamVis - 1;
	if (upper > parameters.size())
	{
		upper = parameters.size();
		lower = 1;
	}
	range = std::make_pair(lower, upper);
}

std::string MadParameterPage::getLatestParameterName()
{
	return "sdfdf";
}

bool MadParameterPage::isEmpty()
{
	return parameters.empty();
}

std::string MadParameterPage::getName()
{
	return this->name;
}

std::list<MadParameter *> *MadParameterPage::getParameters()
{
	return &this->parameters;
}

const std::list<MadParameter *>* MadParameterPage::getParameters() const
{
	return &this->parameters;
}

void MadParameterPage::cycleForward()
{
	if (range.second < parameters.size())
	{
		ofLog() << "Cycling forwards - new range: " << range.first << " to " << range.second;
		unlinkDevice();
		range.first++;
		range.second++;
		linkDevice();
	}
}

void MadParameterPage::cycleBackward()
{
	if (range.first > 1)
	{
		ofLog() << "Cycling backwards - new range: " << range.first << " to " << range.second;
		unlinkDevice();
		range.first--;
		range.second--;
		linkDevice();
	}
}

void MadParameterPage::linkDevice()
{
	auto parameter = parameters.begin();
	for (int i = 1; i < range.first; i++)
	{
		parameter++;
	}

	linkedParamGroup.clear();
	for (int i = 1; i < numParamVis + 1; i++)
	{
		const std::string controlLabel = controlLabelForSlot(midiDevice, i);
		if (parameter != parameters.end())
		{
			if ((*parameter)->getName().empty())
			{
				std::string fallback = (*parameter)->getDisplayParameterName();
				if (fallback.empty()) fallback = "param_" + ofToString(i);
				(*parameter)->setName(fallback);
			}
			if (!controlLabel.empty()) {
				(*parameter)->linkMidiComponent(midiDevice->midiComponents[controlLabel]);
			}
			linkedParamGroup.add(*(*parameter));
			parameter++;
		}
		else
		{
			if (!controlLabel.empty()) {
				midiDevice->midiComponents[controlLabel].value.set(0);
			}
		}
	}

	ofAddListener(linkedParamGroup.parameterChangedE(), this, &MadParameterPage::setValuesOnDevice);
}

void MadParameterPage::unlinkDevice()
{
	auto prevParameter = parameters.begin();
	for (int i = 1; i < range.first; i++)
	{
		prevParameter++;
	}

	for (int i = 1; i < numParamVis + 1 && (prevParameter != parameters.end()); i++)
	{
		const std::string controlLabel = controlLabelForSlot(midiDevice, i);
		if (!controlLabel.empty()) {
			(*prevParameter)->unlinkMidiComponent(midiDevice->midiComponents[controlLabel]);
		}
		prevParameter++;
	}

	ofRemoveListener(linkedParamGroup.parameterChangedE(), this, &MadParameterPage::setValuesOnDevice);
}

std::pair<int, int> MadParameterPage::getRange()
{
	if (!parameters.empty() && range.first == 0)
	{
		int lower = 1;
		int upper = parameters.size();
		if (upper > numParamVis)
			upper = numParamVis;
		range = std::make_pair(lower, upper);
	}
	return range;
}

bool MadParameterPage::isSubpage() { return bSubpage; }
bool MadParameterPage::isGroup() { return bIsGroup; }
void MadParameterPage::setIsGroup(bool isGroup) { bIsGroup = isGroup; }