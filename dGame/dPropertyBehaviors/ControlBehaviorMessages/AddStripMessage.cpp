#include "AddStripMessage.h"

#include "Action.h"

AddStripMessage::AddStripMessage(AMFArrayValue* arguments) : BehaviorMessageBase(arguments) {
	actionContext = ActionContext(arguments);

	position = StripUiPosition(arguments);

	auto* strip = arguments->FindValue<AMFArrayValue>("strip");
	if (!strip) return;

	auto* actions = strip->FindValue<AMFArrayValue>("actions");
	if (!actions) return;

	for (uint32_t actionNumber = 0; actionNumber < actions->GetDenseValueSize(); actionNumber++) {
		auto* actionValue = actions->GetValueAt<AMFArrayValue>(actionNumber);
		if (!actionValue) continue;

		actionsToAdd.push_back(Action(actionValue));

		Game::logger->LogDebug("AddStripMessage", "xPosition %f yPosition %f stripId %i stateId %i behaviorId %i t %s valueParameterName %s valueParameterString %s valueParameterDouble %f", position.GetX(), position.GetY(), actionContext.GetStripId(), actionContext.GetStateId(), behaviorId, actionsToAdd.back().GetType().c_str(), actionsToAdd.back().GetValueParameterName().c_str(), actionsToAdd.back().GetValueParameterString().c_str(), actionsToAdd.back().GetValueParameterDouble());
	}
	Game::logger->Log("AddStripMessage", "number of actions %i", actionsToAdd.size());
}
