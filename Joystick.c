/*
Nintendo Switch Fightstick - Proof-of-Concept

Based on the LUFA library's Low-Level Joystick Demo
	(C) Dean Camera
Based on the HORI's Pokken Tournament Pro Pad design
	(C) HORI

This project implements a modified version of HORI's Pokken Tournament Pro Pad
USB descriptors to allow for the creation of custom controllers for the
Nintendo Switch. This also works to a limited degree on the PS3.

Since System Update v3.0.0, the Nintendo Switch recognizes the Pokken
Tournament Pro Pad as a Pro Controller. Physical design limitations prevent
the Pokken Controller from functioning at the same level as the Pro
Controller. However, by default most of the descriptors are there, with the
exception of Home and Capture. Descriptor modification allows us to unlock
these buttons for our use.
*/

/** \file
 *
 *  Main source file for the Joystick demo. This file contains the main tasks of the demo and
 *  is responsible for the initial application hardware configuration.
 */

#include "Joystick.h"

#define REPS 4

typedef enum {
    SYNC,
    WAIT,
    RUN
} State_t;

/**
 * Truncating buttons to 12 bits HOME and CAPTURE will not work since they are bits 13 and 14
 * respectively. (and bits 15 and 16 are just unused). hat uses only 4 bits so we can safely
 * truncate it 4 bits without loss of capacity. This way it's possible to store buttons + hat
 * into just 2 bytes instead of 3. Making the struct Instruction 3 bytes total instead of 4.
 * Reducing Data size by almost 1/4 due to it's high repetitive use on script vector.
 */
typedef struct {
    uint16_t buttons:12;
    uint8_t hat:4;
    uint8_t duration;
} Instruction;

State_t state = SYNC;

USB_JoystickReport_Input_t last_report;

int reps = 0;
int step = 0;
int duration = 0;

static const Instruction script[] = {
        // Startup
        { SWITCH_NONE         , HAT_CENTER       , 4 },
        { SWITCH_L + SWITCH_R , HAT_CENTER       , 3 },
        { SWITCH_NONE         , HAT_CENTER       , 4 },
        // Unpause
        { SWITCH_PLUS         , HAT_CENTER       , 3 },
        // Run right and open door
        { SWITCH_Y            , HAT_RIGHT        , 17 },
        { SWITCH_Y            , HAT_TOP          , 3 },
        { SWITCH_NONE         , HAT_CENTER       , 20 },
        // Wait descend
        { SWITCH_NONE         , HAT_CENTER       , 90 },
        // Run right and open second door
        { SWITCH_Y            , HAT_RIGHT        , 15 },
        { SWITCH_Y            , HAT_TOP          , 3 },
        { SWITCH_NONE         , HAT_CENTER       , 50 },
        { SWITCH_Y            , HAT_RIGHT        , 13 },
        { SWITCH_Y            , HAT_BOTTOM_RIGHT , 1 },
        // First Jump (Spines)
        { SWITCH_Y + SWITCH_B , HAT_BOTTOM_RIGHT , 4 },
        { SWITCH_Y            , HAT_BOTTOM_RIGHT , 2 },
        { SWITCH_Y            , HAT_BOTTOM       , 4 },
        { SWITCH_Y            , HAT_RIGHT        , 10 },
        // Second Jump (To First P-Switch)
        { SWITCH_Y + SWITCH_B , HAT_CENTER       , 4 },
        { SWITCH_Y + SWITCH_B , HAT_RIGHT        , 4 },
        { SWITCH_Y            , HAT_RIGHT        , 1 },
        { SWITCH_NONE         , HAT_CENTER       , 5 },
        // 1st P-Switch Jump (To Second P-Switch)
        { SWITCH_Y + SWITCH_B , HAT_RIGHT        , 16 },
        { SWITCH_Y            , HAT_CENTER       , 4 },
        // 2nd P-Switch (To platform)
        { SWITCH_Y + SWITCH_B , HAT_LEFT         , 8 },
        { SWITCH_Y            , HAT_CENTER       , 9 },
        // 1st Jump from 1st Platform
        { SWITCH_Y + SWITCH_B , HAT_CENTER       , 4 },
        { SWITCH_Y + SWITCH_B , HAT_LEFT         , 5 },
        { SWITCH_Y            , HAT_LEFT         , 4 },
        { SWITCH_Y            , HAT_RIGHT        , 8 },
        { SWITCH_Y            , HAT_CENTER       , 7 },
        // 2nd Jump from 1st Platform
        { SWITCH_Y + SWITCH_B , HAT_LEFT         , 8 },
        { SWITCH_Y            , HAT_LEFT         , 11 },
        // Jump from 2nd platform
        { SWITCH_Y + SWITCH_B , HAT_LEFT         , 16 },
        { SWITCH_Y            , HAT_LEFT         , 4 },
        // Jump from Shell
        { SWITCH_Y + SWITCH_B , HAT_LEFT         , 50 },
        { SWITCH_Y            , HAT_RIGHT        , 18 },
        // 1st Jump from red block into question block
        { SWITCH_Y + SWITCH_B , HAT_RIGHT        , 1 },
        { SWITCH_Y            , HAT_RIGHT        , 4 },
        { SWITCH_Y            , HAT_LEFT         , 4 },
        // 2st Jump from red block into higher level
        { SWITCH_Y + SWITCH_B , HAT_LEFT         , 25 },
        { SWITCH_PLUS         , HAT_CENTER       , 3 },
//
//        // Do shit
        { SWITCH_NONE         , HAT_CENTER       , 0xFF },
        { SWITCH_NONE         , HAT_CENTER       , 0xFF },
        { SWITCH_NONE         , HAT_CENTER       , 0xFF },
};

// Main entry point.
int main(void) {
    // We'll start by performing hardware and peripheral setup.
    SetupHardware();
    // We'll then enable global interrupts for our use.
    GlobalInterruptEnable();
    // Once that's done, we'll enter an infinite loop.
    for (;;) {
        // We need to run our task to process and deliver data for our IN and OUT endpoints.
        HID_Task();
        // We also need to run the main USB management task.
        USB_USBTask();
    }
}

// Configures hardware and peripherals, such as the USB peripherals.
void SetupHardware(void) {
    // We need to disable watchdog if enabled by bootloader/fuses.
    MCUSR &= ~(1 << WDRF);
    wdt_disable();

    // We need to disable clock division before initializing the USB hardware.
    clock_prescale_set(clock_div_1);
    // We can then initialize our hardware and peripherals, including the USB stack.

    // Both PORTD and PORTB will be used for handling the buttons and stick.
#ifdef USE_PORTS
    DDRD  &= ~0xFF;
    PORTD |=  0xFF;

    DDRB  &= ~0xFF;
    PORTB |=  0xFF;
#endif
    // The USB stack should be initialized last.
    USB_Init();
}

// Fired to indicate that the device is enumerating.
void EVENT_USB_Device_Connect(void) {
    // We can indicate that we're enumerating here (via status LEDs, sound, etc.).
}

// Fired to indicate that the device is no longer connected to a host.
void EVENT_USB_Device_Disconnect(void) {
    // We can indicate that our device is not ready (via status LEDs, sound, etc.).
}

// Fired when the host set the current configuration of the USB device after enumeration.
void EVENT_USB_Device_ConfigurationChanged(void) {
    bool ConfigSuccess = true;

    // We setup the HID report endpoints.
    ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_OUT_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);
    ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_IN_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);

    // We can read ConfigSuccess to indicate a success or failure at this point.
}

// Process control requests sent to the device from the USB host.
void EVENT_USB_Device_ControlRequest(void) {
    // We can handle two control requests: a GetReport and a SetReport.
#ifdef CONTROL_REQUEST
    switch (USB_ControlRequest.bRequest)
    {
        // GetReport is a request for data from the device.
        case HID_REQ_GetReport:
            if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
            {
                // We'll create an empty report.
                USB_JoystickReport_Input_t JoystickInputData;
                // We'll then populate this report with what we want to send to the host.
                GetNextReport(&JoystickInputData);
                // Since this is a control endpoint, we need to clear up the SETUP packet on this endpoint.
                Endpoint_ClearSETUP();
                // Once populated, we can output this data to the host. We do this by first writing the data to the control stream.
                Endpoint_Write_Control_Stream_LE(&JoystickInputData, sizeof(JoystickInputData));
                // We then acknowledge an OUT packet on this endpoint.
                Endpoint_ClearOUT();
            }

            break;
        case HID_REQ_SetReport:
            if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
            {
                // We'll create a place to store our data received from the host.
                USB_JoystickReport_Output_t JoystickOutputData;
                // Since this is a control endpoint, we need to clear up the SETUP packet on this endpoint.
                Endpoint_ClearSETUP();
                // With our report available, we read data from the control stream.
                Endpoint_Read_Control_Stream_LE(&JoystickOutputData, sizeof(JoystickOutputData));
                // We then send an IN packet on this endpoint.
                Endpoint_ClearIN();
            }

            break;
    }
#endif
}

// Process and deliver data from IN and OUT endpoints.
void HID_Task(void) {
    // If the device isn't connected and properly configured, we can't do anything here.
    if (USB_DeviceState != DEVICE_STATE_Configured)
        return;

    // We'll start with the OUT endpoint.
    Endpoint_SelectEndpoint(JOYSTICK_OUT_EPADDR);
    // We'll check to see if we received something on the OUT endpoint.
    if (Endpoint_IsOUTReceived()) {
        // If we did, and the packet has data, we'll react to it.
        if (Endpoint_IsReadWriteAllowed()) {
            // We'll create a place to store our data received from the host.
            USB_JoystickReport_Output_t JoystickOutputData;
            // We'll then take in that data, setting it up in our storage.
#ifdef LOOP_STREAM
            while(Endpoint_Read_Stream_LE(&JoystickOutputData, sizeof(JoystickOutputData), NULL) != ENDPOINT_RWSTREAM_NoError);
#else
            Endpoint_Read_Stream_LE(&JoystickOutputData, sizeof(JoystickOutputData), NULL);
#endif
            // At this point, we can react to this data.
            // However, since we're not doing anything with this data, we abandon it.
        }
        // Regardless of whether we reacted to the data, we acknowledge an OUT packet on this endpoint.
        Endpoint_ClearOUT();
    }

    // We'll then move on to the IN endpoint.
    Endpoint_SelectEndpoint(JOYSTICK_IN_EPADDR);
    // We first check to see if the host is ready to accept data.
    if (Endpoint_IsINReady()) {
        // We'll create an empty report.
        USB_JoystickReport_Input_t JoystickInputData;
        // We'll then populate this report with what we want to send to the host.
        GetNextReport(&JoystickInputData);
        // Once populated, we can output this data to the host. We do this by first writing the data to the control stream.
#ifdef LOOP_STREAM
        while(Endpoint_Write_Stream_LE(&JoystickInputData, sizeof(JoystickInputData), NULL) != ENDPOINT_RWSTREAM_NoError);
#else
        Endpoint_Write_Stream_LE(&JoystickInputData, sizeof(JoystickInputData), NULL);
#endif
        // We then send an IN packet on this endpoint.
        Endpoint_ClearIN();

        /* Clear the report data afterwards */
        // memset(&JoystickInputData, 0, sizeof(JoystickInputData));
    }
}

// Prepare the next report for the host.
void GetNextReport(USB_JoystickReport_Input_t *const ReportData) {

    // Repeat REPS times the last report
    if (reps > 0) {
        memcpy(ReportData, &last_report, sizeof(USB_JoystickReport_Input_t));
        reps--;
        return;
    }

    /* Clear the report contents */
    memset(ReportData, 0, sizeof(USB_JoystickReport_Input_t));
    ReportData->LX = STICK_CENTER;
    ReportData->LY = STICK_CENTER;
    ReportData->RX = STICK_CENTER;
    ReportData->RY = STICK_CENTER;
    ReportData->HAT = HAT_CENTER;

    // States and moves management
    switch (state) {

        case SYNC:
            state = WAIT;
            break;

        case WAIT:
            state = RUN;
            break;

        case RUN:

            ReportData->Button = script[step].buttons;
            ReportData->HAT = script[step].hat;

            duration++;

            if (duration >= script[step].duration) {
                step++;
                duration = 0;
            }

            if (step >= (int) (sizeof(script) / sizeof(script[0]))) // end of script
            {
                return;
            }

            break;
    }

    // Prepare to repeat this report
    memcpy(&last_report, ReportData, sizeof(USB_JoystickReport_Input_t));
    reps = REPS;
}