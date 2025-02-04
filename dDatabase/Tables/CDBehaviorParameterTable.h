#pragma once

// Custom Classes
#include "CDTable.h"
#include <unordered_map>
#include <unordered_set>

/*!
 \file CDBehaviorParameterTable.hpp
 \brief Contains data for the BehaviorParameter table
 */

 //! BehaviorParameter Entry Struct
struct CDBehaviorParameter {
	unsigned int behaviorID;											//!< The Behavior ID
	std::unordered_map<std::string, uint32_t>::iterator parameterID;   	//!< The Parameter ID
	float value;            											//!< The value of the behavior template
};

//! BehaviorParameter table
class CDBehaviorParameterTable : public CDTable {
private:
	std::unordered_map<uint64_t, CDBehaviorParameter> m_Entries;
	std::unordered_map<std::string, uint32_t> m_ParametersList;
public:

	//! Constructor
	CDBehaviorParameterTable(void);

	//! Destructor
	~CDBehaviorParameterTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	float GetValue(const uint32_t behaviorID, const std::string& name, const float defaultValue = 0);

	std::map<std::string, float> GetParametersByBehaviorID(uint32_t behaviorID);
};
