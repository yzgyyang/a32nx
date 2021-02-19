#include <MSFS\MSFS.h>
#include <MSFS\MSFS_WindowsTypes.h>
#include <MSFS\Legacy\gauges.h>
#include <SimConnect.h>

#include <vector>
#include <string>
#include <iostream>
#include <fstream>

//#ifdef _MSC_VER
//#define snprintf _snprintf_s
//#elif !defined(__MINGW32__)
//#include <iconv.h>
//#endif

// https://www.prepar3d.com/SDKv4/sdk/simconnect_api/samples/tagged_data.html

#define lvar_count    1

enum VARBRIDGE_GROUP
{
	DEFAULT
};

enum eEvents
{
	EVENT_FLIGHT_LOADED
};

struct sBridgeVars {
	ENUM m_FcuAltitude;
};

struct TaggedLocalVar {
	int		id;
	float	value;
};

// Holds local variables that the SimConnect client wants to get/read
struct LocalVarsToRead {
	TaggedLocalVar datum[lvar_count];
};

// Holds local variables that the SimConnect client wants to set/write
struct LocalVarsToWrite {
	TaggedLocalVar datum[lvar_count];
};

// Can be changed - maps local variables to integers for TaggedLocalVar id
enum LVAR_NAMES {
	APU_N1,
};

const PCSTRINGZ LVAR_STRINGS[lvar_count][2] = {
	{"A32NX_APU_N", "Percent"},
};

class VarBridge
{
private:
	HANDLE hSimConnect = 0;

	static void CALLBACK s_DispatchProc(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext);
	void DispatchProc(SIMCONNECT_RECV* pData, DWORD cbData);
	void RegisterEvents();
	void RegisterVariables();

public:
	bool Initialize();
	bool OnFrameUpdate();
	bool Quit();
};

enum ClientData {
	WriteToSim = 0,
	ReadFromSim = 1,
};


VarBridge GLOBAL_VARBRIDGE;
ID ID_LSIMVAR[lvar_count];
const char* CustomEventPrefix = "VarBridge.";

std::vector<std::string> Events = {
	"XMLVAR_Baro1_Mode",
	"A320_Neo_MFD_BTN_LS_1",
	"A320_Neo_MFD_BTN_CSTR_1",
	"A320_Neo_MFD_BTN_WPT_1",
	"A320_Neo_MFD_BTN_VORD_1",
	"A320_Neo_MFD_BTN_NDB_1",
	"A320_Neo_MFD_BTN_ARPT_1",
	"A320_Neo_MFD_NAV_MODE_1",
	"A320_Neo_MFD_Range_1",
	"XMLVAR_NAV_AID_SWITCH_L1_State",
	"XMLVAR_NAV_AID_SWITCH_L2_State",
	"AP_MANAGED_SPEED_IN_MACH_TOGGLE",
	"A320_Neo_CDU_MODE_MANAGED_SPEED",
	"A320_Neo_FCU_MODE_SELECTED_SPEED",
	"A320_Neo_CDU_MODE_MANAGED_SPEED",
	"A320_Neo_FCU_MODE_SELECTED_SPEED",
	"A32NX_TRK_FPA_MODE_ACTIVE",
	"A32NX_AUTOPILOT_LOC_MODE",
	"A32NX_AUTOPILOT_APPR_MODE",
	"A320_Neo_EXPEDITE_MODE",
	"A32NX_METRIC_ALT_TOGGLE",
	"XMLVAR_Autopilot_Altitude_Increment",
};

void VarBridge::RegisterEvents() {
	DWORD eventID = 0;
	for (const auto& value : Events) {
		std::string eventCommand = value;
		std::string eventName = std::string(CustomEventPrefix) + eventCommand;

		HRESULT hr = SimConnect_MapClientEventToSimEvent(hSimConnect, eventID, eventName.c_str());
		hr = SimConnect_AddClientEventToNotificationGroup(hSimConnect, VARBRIDGE_GROUP::DEFAULT, eventID, false);
		eventID++;
	}
	SimConnect_SetNotificationGroupPriority(hSimConnect, VARBRIDGE_GROUP::DEFAULT, SIMCONNECT_GROUP_PRIORITY_HIGHEST);
}

void VarBridge::RegisterVariables() {
	for (int i = 0; i < lvar_count; i++) {
		ID_LSIMVAR[i] = register_named_variable(LVAR_STRINGS[i][0]);
	}
	printf("Registered variables!\n");
}

void CALLBACK VarBridge::s_DispatchProc(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext)
{
	GLOBAL_VARBRIDGE.DispatchProc(pData, cbData);
}

bool VarBridge::Initialize()
{
	if (SUCCEEDED(SimConnect_Open(&hSimConnect, "VarBridge", nullptr, 0, 0, 0)))
	{
		printf("### SimConnect connected.\r\n");

		// These two lines added from event code
		SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_FLIGHT_LOADED, "FlightLoaded");
		GLOBAL_VARBRIDGE.RegisterEvents();
		GLOBAL_VARBRIDGE.RegisterVariables();

		SimConnect_MapClientDataNameToID(hSimConnect, "ReadFromSim", ClientData::ReadFromSim);
		SimConnect_MapClientDataNameToID(hSimConnect, "WriteToSim", ClientData::WriteToSim);

		SimConnect_CreateClientData(hSimConnect,
			ClientData::ReadFromSim,
			sizeof(LocalVarsToRead),
			SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);

		SimConnect_CreateClientData(hSimConnect,
			ClientData::WriteToSim,
			sizeof(LocalVarsToWrite),
			SIMCONNECT_CREATE_CLIENT_DATA_FLAG_DEFAULT);

		SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::ReadFromSim, 0, sizeof(LocalVarsToRead), 0, 0);
		SimConnect_AddToClientDataDefinition(hSimConnect, ClientData::WriteToSim, 0, sizeof(LocalVarsToWrite), 0, 0);

		// Gets data to write from client, which is then caught by dispatch procedure
		SimConnect_RequestClientData(hSimConnect,
			ClientData::WriteToSim,
			0,
			0,
			SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET,
			SIMCONNECT_CLIENT_DATA_REQUEST_FLAG_CHANGED,
			0,
			0,
			0);

		SimConnect_CallDispatch(hSimConnect, s_DispatchProc, static_cast<VarBridge*>(this));

		printf("### SimConnect registrations complete.\r\n");
		return true;
	}
	else {
		printf("### SimConnect failed.\r\n");
	}

	return true;
}

bool VarBridge::OnFrameUpdate()
{
	// Gets data to read from sim, then puts it in client data area
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Iterate through each variable to read, get named variable, and put in client data area
	///////////////////////////////////////////////////////////////////////////////////////////////

	LocalVarsToRead* sR = new LocalVarsToRead();

	for (int i = 0; i < lvar_count; i++) {
		sR->datum[i].id = i;
		sR->datum[i].value = get_named_variable_value(ID_LSIMVAR[i]);
		printf("Retrieved variable %s - value: %.2f\n", LVAR_STRINGS[i][0], sR->datum[i].value);
	}

	SimConnect_SetClientData(hSimConnect,
		ClientData::ReadFromSim,
		ClientData::ReadFromSim,
		SIMCONNECT_CLIENT_DATA_SET_FLAG_DEFAULT,
		0,
		sizeof(LocalVarsToRead),
		sR
	);

	// quick n dirty temp garbage collection
	delete sR;

	return true;
}

bool VarBridge::Quit()
{
	return SUCCEEDED(SimConnect_Close(hSimConnect));
}

void VarBridge::DispatchProc(SIMCONNECT_RECV* pData, DWORD cbData)
{
	printf("### SimConnect DispatchProc\r\n");

	switch (pData->dwID) {

		case SIMCONNECT_RECV_ID::SIMCONNECT_RECV_ID_EVENT: {
			SIMCONNECT_RECV_EVENT* evt = (SIMCONNECT_RECV_EVENT*)pData;
			int eventID = evt->uEventID;

			if (eventID >= Events.size()) {
				fprintf(stderr, "VarBridge: EventID out of range: %u\n", eventID);
				break;
			}

			std::string command = std::string("(>H:") + std::string(Events[eventID]) + std::string(")");
			execute_calculator_code(command.c_str(), nullptr, nullptr, nullptr);
			fprintf(stderr, "%s\n", command.c_str());
		}
		break;

		case SIMCONNECT_RECV_ID::SIMCONNECT_RECV_ID_CLIENT_DATA: {
			printf("### SimConnect CLIENT_DATA\r\n");

			auto recv_data = static_cast<SIMCONNECT_RECV_CLIENT_DATA*>(pData);

			//////////////////////////////////////////////////////////////////////////////////////////////
			// Iterate through each variable in received simvars from client, and set named variable
			//////////////////////////////////////////////////////////////////////////////////////////////

			LocalVarsToWrite* sW = (LocalVarsToWrite*)&recv_data->dwData;

			int count = 0;
			while (count < (int)recv_data->dwDefineCount) {
				set_named_variable_value(ID_LSIMVAR[count], sW->datum[count].value);
				++count;
			}

		}
		break;

		case SIMCONNECT_RECV_ID::SIMCONNECT_RECV_ID_EXCEPTION: {
			SIMCONNECT_RECV_EXCEPTION* ex = static_cast<SIMCONNECT_RECV_EXCEPTION*>(pData);
			printf("### SimConnect EXCEPTION: %d \r\n", ex->dwException);
		}
		break;

		default:
			break;
	}
}

extern "C" {

	MSFS_CALLBACK bool VarBridge_gauge_callback(FsContext ctx, int service_id, void* pData)
	{
		switch (service_id)
		{
		case PANEL_SERVICE_PRE_INSTALL:
			return true;
			break;
		case PANEL_SERVICE_POST_INSTALL:
			return GLOBAL_VARBRIDGE.Initialize();
			break;
		case PANEL_SERVICE_PRE_DRAW:
			return GLOBAL_VARBRIDGE.OnFrameUpdate();
			break;
		case PANEL_SERVICE_PRE_KILL:
			return GLOBAL_VARBRIDGE.Quit();
			break;
		}
		return false;
	}

}