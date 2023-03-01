#pragma once

#include <cppapi/IImplant.h>
#include <cppapi/IImplantListener.h>
#include <cppapi/IStimulationCommand.h>
#include "gtest/gtest.h"
#include "gmock/gmock.h"



class MockIImplant : public cortec::implantapi::IImplant {
public:
	MockIImplant();
	virtual ~MockIImplant();
	MOCK_METHOD(void, registerListener, (cortec::implantapi::IImplantListener* listener), (override));
	MOCK_METHOD(cortec::implantapi::CImplantInfo*, getImplantInfo, (), (const, override));
	MOCK_METHOD(void, startMeasurement, (const std::set<uint32_t>& refChannels,
		const cortec::implantapi::RecordingAmplificationFactor amplificationFactor,
		const bool useGndElectrode), (override));
	MOCK_METHOD(void, stopMeasurement, (), (override));
	MOCK_METHOD(double, getImpedance, (const uint32_t channel), (override));
	MOCK_METHOD(double, getTemperature, (), (override));
	MOCK_METHOD(double, getHumidity, (), (override));
	MOCK_METHOD(bool, isStimulationCommandValid, (const cortec::implantapi::IStimulationCommand* cmd, std::string* message), (override));
	MOCK_METHOD(std::vector<cortec::implantapi::IStimulationCommand::StimulationCommandFault>, enqueueStimulationCommand, (const cortec::implantapi::IStimulationCommand* cmd, const cortec::implantapi::StimulationMode mode), (override));
	MOCK_METHOD(void, startStimulation, (const uint8_t stimulationFunctionID), (override));
	MOCK_METHOD(void, startStimulation, (), (override));
	MOCK_METHOD(void, stopStimulation, (), (override));
	MOCK_METHOD(void, setImplantPower, (const bool enabled), (override));
	MOCK_METHOD(void, pushState, (), (override));
	MOCK_METHOD(void, setRFQualityPolling, (const bool enable), (override));

};
