#pragma once

#include <interpreterBase/robotModel/robotParts/lightSensor.h>

#include "commonTwoDModel/commonTwoDModelDeclSpec.h"

namespace twoDModel {

namespace engine {
class TwoDModelEngineInterface;
}

namespace robotModel {
namespace parts {

class COMMON_TWO_D_MODEL_EXPORT LightSensor : public interpreterBase::robotModel::robotParts::LightSensor
{
	Q_OBJECT

public:
	LightSensor(interpreterBase::robotModel::DeviceInfo const &info
			, interpreterBase::robotModel::PortInfo const &port
			, engine::TwoDModelEngineInterface &engine);

	void read() override;

protected:
	void doConfiguration() override;

private:
	engine::TwoDModelEngineInterface &mEngine;
};

}
}
}