// Please read VController_v3.ino for information about the license and authors

// This page has the following parts:
// Section 1: Switch Action Trigger and Command Lookup
// Section 2: Command Execution
// Section 3: Parameter State Control
// Section 4: MIDI CC Commands
// Section 5: Page Selection Commands
// Section 6: Global Tuner Commands
// Section 7: Global Tap Tempo Commands
// Section 8: Bass Mode (Low String Priority)
// Section 9: VController Power On/Off Switching

// ********************************* Section 1: Switch Action Trigger and Command Lookup ********************************************

// Here an action is taken after a switch is pressed. This action is read from the config file
bool updown_direction_can_change;

// Memory buffer for commands
#define CMD_BUFFER_SIZE 50
Cmd_struct cmd_buf[CMD_BUFFER_SIZE]; // We store fifty commands to allow for faster access
uint8_t current_cmdbuf_index = 0; // Index to the cmd_buf array

uint8_t prev_switch_pressed = 0; // To allow detection of re-pressing a switch
uint16_t current_cmd = 0; // The current command that is being executed
uint8_t current_cmd_switch = 0; // Placeholder for the current_switch where the action is taken on

uint8_t current_cmd_switch_action = 0; // Placeholder for the switch action of the current command. Types are below:
#define SWITCH_PRESSED 1
#define SWITCH_PRESSED_REPEAT 2
#define SWITCH_RELEASED 3
#define SWITCH_LONG_PRESSED 4
#define SWITCH_HELD 5

uint8_t arm_page_cmd_exec = 0; // Set to the number of the selected page to execute the commands for this page

void setup_switch_control()
{
  //SCO_reset_all_switch_states();
  //PAGE_load_current(true);
}

// Take action on switch being pressed / released/ held / long pressed or extra long pressed.
// There can be any number of commands executed by one switch press. The maximum number of commands for one switch is determined by the size of the buffer (CMD_BUFFER_SIZE).
// On every loop cycle one command is executed. THe next command to be executed is set in the current_cmd variable.
// Commands read from EEPROM are stored in a memory buffer (cmd_buf) in RAM to increase performance.

void main_switch_control()  // Checks if a button has been pressed and check out which functions have to be executed
{
  if (switch_released > 0) { // When switch is released, set current_cmd_switch_action to SWITCH_RELEASED and let current_cmd point to the first command for this switch
    SP[switch_released].Pressed = false;
    update_LEDS = true;
    current_cmd_switch_action = SWITCH_RELEASED;
    current_cmd = EEPROM_first_cmd(Current_page, switch_released);
    current_cmd_switch = switch_released;
    current_cmdbuf_index = 0;
  }

  if (switch_pressed > 0) {
    SP[switch_pressed].Pressed = true;
    update_LEDS = true;
    // Check if we are in tuner mode - pressing any key will stop tuner mode
    if (global_tuner_active) {
      SCO_global_tuner_stop();
    }
    else { // When switch is pressed, set current_cmd_switch_action to SWITCH_PRESSED or SWITCH_PRESSED_REPEAT and let current_cmd point to the first command for this switch
      if ((switch_pressed == prev_switch_pressed) && (switch_is_expression_pedal)) {
        current_cmd_switch_action = SWITCH_PRESSED_REPEAT;
      }
      else {
        current_cmd_switch_action = SWITCH_PRESSED;
        prev_switch_pressed = switch_pressed;
      }
      current_cmd = EEPROM_first_cmd(Current_page, switch_pressed);
      DEBUGMSG("Current cmd:" + String(current_cmd) + " on page:" + String(Current_page));
      current_cmd_switch = switch_pressed;
      current_cmdbuf_index = 0;
    }
  }

  if (switch_long_pressed > 0) { // When switch is long pressed, set current_cmd_switch_action to SWITCH_LONG_PRESSED and let current_cmd point to the first command for this switch
    //SCO_switch_long_pressed_commands(Current_page, switch_long_pressed);
    current_cmd_switch_action = SWITCH_LONG_PRESSED;
    current_cmd = EEPROM_first_cmd(Current_page, switch_long_pressed);
    current_cmd_switch = switch_long_pressed;
    current_cmdbuf_index = 0;
  }

  if (switch_held > 0) { // When switch is held, set current_cmd_switch_action to SWITCH_HELD and let current_cmd point to the first command for this switch
    //SCO_switch_held_commands(Current_page, switch_held);
    current_cmd_switch_action = SWITCH_HELD;
    current_cmd = EEPROM_first_cmd(Current_page, switch_held);
    current_cmd_switch = switch_held;
    current_cmdbuf_index = 0;
  }

#ifdef POWER_SWITCH_NUMBER
  if (switch_extra_long_pressed == POWER_SWITCH_NUMBER) { // When switch is extra long pressed and it is the power switch, turn off the VController
    SCO_select_page(Previous_page, Previous_device); // "Undo"
    SCO_switch_power_off();
  }
#endif

  if (current_cmd > 0) { // If current_cmd points to a command we can execute, do it. Then check if there is another command to execute.
    SCO_execute_cmd(current_cmd_switch, current_cmd_switch_action, current_cmdbuf_index);
    current_cmd = EEPROM_next_cmd(current_cmd); //Find the next command - will be executed on the next cycle
    current_cmdbuf_index++; // Point to the next command in the command buffer
    if (current_cmdbuf_index >= CMD_BUFFER_SIZE) current_cmd = 0; // Stop executing commands when the end of the buffer is reached.
  }

  if ((current_cmd == 0) && (arm_page_cmd_exec > 0)) { // Execute any commands on page selection
    // We do this when current_cmd is 0, because the arm_page_exec variable is set from another command that is running.
    current_cmd_switch_action = SWITCH_PRESSED;
    prev_switch_pressed = 0;
    current_cmd = EEPROM_first_cmd(arm_page_cmd_exec, 0);
    DEBUGMSG("Trigger execution of default command for page " + String(arm_page_cmd_exec));
    current_cmd_switch = 0;
    current_cmdbuf_index = 0;
    arm_page_cmd_exec = 0;
  }

  SCO_update_tap_tempo_LED();
}

// ********************************* Section 2: Command Execution ********************************************

void SCO_execute_cmd(uint8_t sw, uint8_t action, uint8_t index) {
  switch (action) {
    case SWITCH_PRESSED:
      // Here we read the switch and store the value in the command buffer. This is only done on first press. After that the commands are executed from the buffer.
      // This allows for smoother expression pedal operation, where we trigger the same commands in quick succesion
      read_cmd_EEPROM(current_cmd, &cmd_buf[index]);
    // No break!
    case SWITCH_PRESSED_REPEAT:
      SCO_execute_command_press(sw, &cmd_buf[index], (index == 0));
      break;
    case SWITCH_RELEASED:
      SCO_execute_command_release(sw, &cmd_buf[index], (index == 0));
      break;
    case SWITCH_LONG_PRESSED:
      SCO_execute_command_long_pressed(sw, &cmd_buf[index], (index == 0));
      break;
    case SWITCH_HELD:
      SCO_execute_command_held(sw, &cmd_buf[index], (index == 0));
      break;
  }
}

void SCO_trigger_default_page_cmds(uint8_t Pg) {
  arm_page_cmd_exec = Pg;
}

void SCO_execute_command_press(uint8_t Sw, Cmd_struct *cmd, bool first_cmd) {

  uint8_t Dev = cmd->Device;
  if (Dev == CURRENT) Dev = Current_device;
  uint8_t Type = cmd->Type;
  uint8_t Data1 = cmd->Data1;
  uint8_t Data2 = cmd->Data2;
  uint16_t new_patch;
  uint8_t New_page;

  DEBUGMAIN("Press -> execute command " + String(Type) + " for device " + String(Dev));

  if (Dev == COMMON) { // Check for common parameters
    switch (Type) {
      case TAP_TEMPO:
        SCO_global_tap_tempo_press();
        break;
      case SET_TEMPO:
        SCO_set_global_tempo_press(Data1);
        break;
      case OPEN_PAGE:
        SCO_trigger_default_page_cmds(Data1);
        SCO_select_page(Data1);
        break;
      case PAGE_UP:
        if (Current_page < (Number_of_pages - 1)) New_page = Current_page + 1;
        else New_page = LOWEST_USER_PAGE;
        SCO_trigger_default_page_cmds(New_page);
        SCO_select_page(New_page);
        break;
      case PAGE_DOWN:
        if (Current_page > LOWEST_USER_PAGE) New_page = Current_page - 1;
        else New_page = Number_of_pages - 1;
        SCO_trigger_default_page_cmds(New_page);
        SCO_select_page(New_page);
        break;
      case MENU:
        menu_press(Sw);
        break;
      case SELECT_NEXT_DEVICE:
        SCO_select_next_device();
        break;
      case GLOBAL_TUNER:
        SCO_global_tuner_toggle();
        break;
      case MIDI_CC:
        SCO_send_CC(Data1, Data2, cmd->Value1, cmd->Value2, cmd->Value3, SCO_MIDI_port(cmd->Value4), Sw, first_cmd);
        if (!switch_is_expression_pedal) update_page = REFRESH_PAGE;
        //if (data2 == RANGE) {
        //  MIDI_send_CC(Data1, Data2, SCO_return_parameter_value(Sw, Cmd)); // Controller, Value, Channel, Port
        //}
        break;
      case MIDI_PC:
        MIDI_send_PC(Data1, Data2, SCO_MIDI_port(cmd->Value1));
        MIDI_remember_PC(Data1, Data2, SCO_MIDI_port(cmd->Value1));
        break;
      case MIDI_NOTE:
        MIDI_send_note_on(Data1, Data2, cmd->Value1, SCO_MIDI_port(cmd->Value2));
        break;
    }
  }
  if (Dev < NUMBER_OF_DEVICES) { // Check for device specific parameters
    switch (Type) {
      case PATCH_SEL:
        Device[Dev]->patch_select(Data1 + (Data2 * 100));
        Device[Dev]->current_patch_name = SP[Sw].Label; // Store current patch name
        mute_all_but_me(Dev); // mute all the other devices
        break;
      case PATCH_BANK:
        Device[Dev]->patch_select(SP[Sw].PP_number);
        Device[Dev]->current_patch_name = SP[Sw].Label; // Store current patch name
        mute_all_but_me(Dev); // mute all the other devices
        DEBUGMSG("Selecting patch: " + String(SP[Sw].PP_number));
        break;
      case PREV_PATCH:
        new_patch = Device[Dev]->prev_patch_number();
        Device[Dev]->patch_select(new_patch);
        Device[Dev]->current_patch_name = SP[Sw].Label; // Store current patch name
        Device[Dev]->request_current_patch_name(); // Request the correct patch name
        mute_all_but_me(Dev); // mute all the other devices
        DEBUGMSG("Selecting patch: " + String(SP[Sw].PP_number));
        break;
      case NEXT_PATCH:
        new_patch = Device[Dev]->next_patch_number();
        Device[Dev]->patch_select(new_patch);
        Device[Dev]->current_patch_name = SP[Sw].Label; // Store current patch name
        Device[Dev]->request_current_patch_name(); // Request the correct patch name
        mute_all_but_me(Dev); // mute all the other devices
        DEBUGMSG("Selecting patch: " + String(SP[Sw].PP_number));
        break;
      case DIRECT_SELECT:
        Device[Dev]->direct_select_press(Data1);
        update_page = RELOAD_PAGE;
        break;
      case BANK_UP:
        Device[Dev]->bank_updown(UP, Data1);
        update_main_lcd = true;
        update_page = REFRESH_PAGE;
        break;
      case BANK_DOWN:
        Device[Dev]->bank_updown(DOWN, Data1);
        update_main_lcd = true;
        update_page = REFRESH_PAGE;
        break;
      case PARAMETER:
        if (first_cmd) SCO_update_parameter_state(Sw, cmd->Value1, cmd->Value2, cmd->Value3); // Passing min, max and step value for STEP, RANGE and UPDOWN style pedal
        Device[Dev]->parameter_press(Sw, cmd, Data1);
        break;
      case PAR_BANK:
        if (first_cmd) SCO_update_parameter_state(Sw, 0, Device[Dev]->number_of_values(SP[Sw].PP_number) - 1, 1); // Passing min, max and step value for STEP, RANGE and UPDOWN style pedal
        Device[Dev]->parameter_press(Sw, cmd, SP[Sw].PP_number);
        break;
      case PAR_BANK_UP:
        Device[Dev]->par_bank_updown(UP, Data1);
        update_main_lcd = true;
        break;
      case PAR_BANK_DOWN:
        Device[Dev]->par_bank_updown(DOWN, Data1);
        update_main_lcd = true;
        break;
      case ASSIGN:
        if (first_cmd) SCO_update_parameter_state(Sw, 0, 1, 1);
        if (switch_is_expression_pedal) Device[Dev]->assign_press(Sw, Expr_ped_value);
        else Device[Dev]->assign_press(Sw, 127);
        break;
      case MUTE:
        Device[Dev]->mute();
        update_LEDS = true;
        break;
      case OPEN_PAGE_DEVICE:
        SCO_trigger_default_page_cmds(Data1);
        SCO_select_page(Data1, Dev);
        break;
      case OPEN_PAGE_PATCH:
        SCO_trigger_default_page_cmds(Data1);
        SCO_select_patch_page(Dev);
        break;
      case OPEN_PAGE_PARAMETER:
        SCO_trigger_default_page_cmds(Data1);
        SCO_select_parameter_page(Dev);
        break;
      case OPEN_PAGE_ASSIGN:
        SCO_trigger_default_page_cmds(Data1);
        SCO_select_assign_page(Dev);
        break;
    }
  }
}

void SCO_execute_command_release(uint8_t Sw, Cmd_struct *cmd, bool first_cmd) {
  uint8_t Dev = cmd->Device;
  if (Dev == CURRENT) Dev = Current_device;
  uint8_t Type = cmd->Type;
  uint8_t Data1 = cmd->Data1;
  uint8_t Data2 = cmd->Data2;

  DEBUGMSG("Release -> execute command " + String(Type) + " for device " + String(Dev));

  if (Dev == COMMON) { // Check for common parameters
    switch (Type) {
      case MIDI_NOTE:
        MIDI_send_note_off(Data1, cmd->Data2, cmd->Value1, SCO_MIDI_port(cmd->Value2));
        break;
      case MIDI_CC:
        if (cmd->Data2 == CC_MOMENTARY) {
          MIDI_send_CC(Data1, cmd->Value2, cmd->Value3, SCO_MIDI_port(cmd->Value4)); // Controller, Value, Channel, Port
          MIDI_remember_CC(Data1, cmd->Value2, cmd->Value3, SCO_MIDI_port(cmd->Value4));
        }
        if ((cmd->Data2 == CC_UPDOWN) && (updown_direction_can_change)) {
          SP[Sw].Direction ^= 1; // Toggle direction
          update_lcd = Sw;
        }
        break;
    }
  }
  if (Dev < NUMBER_OF_DEVICES) {
    switch (Type) {
      case PARAMETER:
        if (first_cmd) SCO_update_released_parameter_state(Sw, 0, Device[Dev]->number_of_values(SP[Sw].PP_number) - 1, 1); // Passing min, max and step value for STEP, RANGE and UPDOWN style pedal
        Device[Dev]->parameter_release(Sw, cmd, Data1 + (Data2 * 100));
        break;
      case PAR_BANK:
        if (first_cmd) SCO_update_released_parameter_state(Sw, 0, Device[Dev]->number_of_values(SP[Sw].PP_number) - 1, 1); // Passing min, max and step value for STEP, RANGE and UPDOWN style pedal
        Device[Dev]->parameter_release(Sw, cmd, SP[Sw].PP_number);
        break;
      case ASSIGN:
        //case ASG_BANK:
        if (first_cmd) SCO_update_released_parameter_state(Sw, 0, Device[Dev]->number_of_values(SP[Sw].PP_number) - 1, 1); // Passing min, max and step value for STEP, RANGE and UPDOWN style pedal
        Device[Dev]->assign_release(Sw);
        break;
    }
  }
}

void SCO_execute_command_long_pressed(uint8_t Sw, Cmd_struct *cmd, bool first_cmd) {

  uint8_t Dev = cmd->Device;
  if (Dev == CURRENT) Dev = Current_device;
  uint8_t Type = cmd->Type;

  DEBUGMSG("Long press -> execute command " + String(Type) + " for device " + String(Dev));

  switch (Type) {
    case OPEN_PAGE: // Go to user select page on long press of any page command
    case OPEN_PAGE_DEVICE:
    case SELECT_NEXT_DEVICE:
    case OPEN_PAGE_PATCH:
    case OPEN_PAGE_PARAMETER:
    case OPEN_PAGE_ASSIGN:
    case PAGE_UP:
    case PAGE_DOWN:
      Current_page = Previous_page; // "Undo" for pressing OPEN_PAGE or SELECT_NEXT_DEVICE
      Current_device = Previous_device;
      SCO_select_page(PAGE_USER_SELECT);
      prev_page_shown = Previous_page; // Page to be displayed on PAGE_SELECT LEDS
      break;
    case BANK_DOWN: // Go to direct select on long press of bank up/down or patch up/down
    case BANK_UP:
    case PREV_PATCH:
    case NEXT_PATCH:
      device_in_bank_selection = 0; // Switch off bank selection
      Previous_bank_size = cmd->Data1; // Remember the bank size
      if (Dev < NUMBER_OF_DEVICES) Device[Dev]->bank_select_number = 0; // Reset bank number
      SCO_select_page(PAGE_CURRENT_DIRECT_SELECT); // Jump to the direct select page
      break;
    case TAP_TEMPO:
      SCO_global_tuner_toggle(); //Start global tuner
      break;
  }
}

void SCO_execute_command_held(uint8_t Sw, Cmd_struct *cmd, bool first_cmd) {
  uint8_t Dev = cmd->Device;
  if (Dev == CURRENT) Dev = Current_device;
  uint8_t Type = cmd->Type;
  uint8_t Data1 = cmd->Data1;
  uint8_t Data2 = cmd->Data2;

  DEBUGMSG("Switch held -> execute command " + String(Type) + " for device " + String(Dev));

  if (Dev < NUMBER_OF_DEVICES) {
    switch (Type) {
      case PARAMETER:
        if (Data2 == UPDOWN) {
          if (first_cmd) SCO_update_held_parameter_state(Sw, 0, Device[Dev]->number_of_values(SP[Sw].PP_number) - 1, 1); // Passing min, max and step value for STEP, RANGE and UPDOWN style pedal
          Device[Dev]->parameter_press(Sw, cmd, Data1);
          updown_direction_can_change = false;
        }
        break;
      case PAR_BANK:
        if (SP[Sw].Latch == UPDOWN) {
          if (first_cmd) SCO_update_held_parameter_state(Sw, 0, Device[Dev]->number_of_values(SP[Sw].PP_number) - 1, 1); // Passing min, max and step value for STEP, RANGE and UPDOWN style pedal
          Device[Dev]->parameter_press(Sw, cmd, SP[Sw].PP_number);
          updown_direction_can_change = false;
        }
        break;
    }
  }
  if (Dev == COMMON) {
    switch (Type) {
      case MENU:
        menu_press_hold(Sw);
        break;
      case MIDI_CC:
        if (Data2 == CC_UPDOWN) {
          if (SP[Sw].Direction == UP) {
            if (SP[Sw].Target_byte1 < cmd->Value1) SP[Sw].Target_byte1++;
          }
          else {
            if (SP[Sw].Target_byte1 > cmd->Value2) SP[Sw].Target_byte1--;
          }
          MIDI_send_CC(Data1, SP[Sw].Target_byte1, cmd->Value3, SCO_MIDI_port(cmd->Value4)); // Controller, Value, Channel, Port
          MIDI_remember_CC(Data1, SP[Sw].Target_byte1, cmd->Value3, SCO_MIDI_port(cmd->Value4));
          update_lcd = Sw;
          updown_direction_can_change = false;
        }
        break;
    }
  }
}

void mute_all_but_me(uint8_t me_device) {
  for (uint8_t d = 0; d < NUMBER_OF_DEVICES; d++) {
    if (d != me_device) Device[d]->mute();
  }
}

// ********************************* Section 3: Parameter State Control ********************************************

void SCO_update_parameter_state(uint8_t Sw, uint8_t Min, uint8_t Max, uint8_t Step) {
  uint8_t val;

  if (switch_is_expression_pedal) {
    SP[Sw].State = 1;
    if (SP[Sw].Latch == RANGE) SP[Sw].Target_byte1 = map(Expr_ped_value, 0, 127, Min, Max);
    LCD_show_bar(Expr_ped_value); // Show it on the main display
  }
  else {

    switch (SP[Sw].Latch) {
      case MOMENTARY:
        SP[Sw].State = 1; // Switch state on
        break;
      case TOGGLE:  // Toggle state
        SP[Sw].State++;
        if (SP[Sw].State > 2) SP[Sw].State = 1;
        break;
      case TRISTATE:  // Select next state
        SP[Sw].State++;
        if (SP[Sw].State > 3) SP[Sw].State = 1;
        break;
      case FOURSTATE:  // Select next state
        SP[Sw].State++;
        if (SP[Sw].State > 4) SP[Sw].State = 1;
        break;
      //case FIVESTATE: // Select next state
      //  SP[Sw].State++;
      //  if (SP[Sw].State > 5) SP[Sw].State = 1;
      //  break;
      case STEP:
        // Update byte1 with the new value
        val = SP[Sw].Target_byte1;
        val += Step;
        if (val > Max) val = Min;
        SP[Sw].Target_byte1 = val;
        break;
      case UPDOWN:
        updown_direction_can_change = true;
        break;
    }
    DEBUGMSG("New state is " + String(SP[Sw].State));
  }
}

void SCO_update_released_parameter_state(uint8_t Sw, uint8_t Min, uint8_t Max, uint8_t Step) {
  if ((SP[Sw].Latch == UPDOWN) && (updown_direction_can_change)) {
    SP[Sw].Direction ^= 1; // Toggle direction
    update_lcd = Sw;
  }
  if (SP[Sw].Latch == MOMENTARY) SP[Sw].State = 2;
}

void SCO_update_held_parameter_state(uint8_t Sw, uint8_t Min, uint8_t Max, uint8_t Step) {
  uint8_t val;
  if (SP[Sw].Latch == UPDOWN) {
    // Update byte1 with the new value
    val = SP[Sw].Target_byte1;
    if (SP[Sw].Direction) {
      if (val < Max) val++;
    }
    else {
      if (val > 0) val--;
    }
    SP[Sw].Target_byte1 = val;
  }
}

uint8_t SCO_return_parameter_value(uint8_t Sw, Cmd_struct * cmd) {
  if (SP[Sw].Latch == RANGE) return SP[Sw].Target_byte1;
  if (SP[Sw].Latch == STEP) return SP[Sw].Target_byte1;
  if (SP[Sw].Latch == UPDOWN) return SP[Sw].Target_byte1;
  if (SP[Sw].Type == PARAMETER) { //Parameters are read directly from the switch config.
    if (SP[Sw].State == 1) return cmd->Value1;
    if (SP[Sw].State == 2) return cmd->Value2;
    if (SP[Sw].State == 3) return cmd->Value3;
    if (SP[Sw].State == 4) return cmd->Value4;
    //if (SP[Sw].State == 5) return cmd->Value5;
  }
  else { //Return values from the SP array
    if (SP[Sw].State == 1) return 1;
    if (SP[Sw].State == 2) return 0;
  }
  return 0;
}

uint8_t SCO_find_parameter_state(uint8_t Sw, uint8_t value) {
  //Cmd_struct cmd;
  //EEPROM_read_cmd(Current_page, Sw, 0, &cmd);
  switch (SP[Sw].Type) {
    case PARAMETER:
      if (value == SP[Sw].Value1) return 1;
      if (value == SP[Sw].Value2) return 2;
      if (value == SP[Sw].Value3) return 3;
      if (value == SP[Sw].Value4) return 4;
      //if (value == SP[Sw].Value5) return 5;
      return 0;

    case PAR_BANK:
      if (value == 0) return 2;
      if (value == 1) return 1;
      return 0;

    case ASSIGN:
      if (value == SP[Sw].Assign_min) return 2;
      if (value == SP[Sw].Assign_max) return 1;
      return 0;
  }
  return 0;
}

// ********************************* Section 4: MIDI CC Commands ********************************************

uint8_t SCO_MIDI_port(uint8_t port) { // Converts the port number from the menu to the proper port number
  if (port < NUMBER_OF_MIDI_PORTS) return port << 4;
  else return ALL_PORTS;
}

void SCO_send_CC(uint8_t CC_number, uint8_t CC_toggle, uint8_t value1, uint8_t value2, uint8_t channel, uint8_t port, uint8_t Sw, bool first_cmd) {

  uint8_t val;

  if (switch_is_expression_pedal) {
    if (SP[Sw].Latch == CC_RANGE) {
      MIDI_send_CC(CC_number, map(Expr_ped_value, 0, 127, value2, value1), channel, port); // Controller, Value, Channel, Port;
      MIDI_remember_CC(CC_number, value1, channel, port);
      LCD_show_bar(Expr_ped_value); // Show it on the main display
      if (CC_number < 10) LCD_show_status_message("CC #00" + String(CC_number));
      else if (CC_number < 100) LCD_show_status_message("CC #0" + String(CC_number));
      else LCD_show_status_message("CC #0" + String(CC_number));
    }
  }
  else {
    switch (CC_toggle) {
      case CC_ONE_SHOT:
      case CC_MOMENTARY:
        MIDI_send_CC(CC_number, value1, channel, port); // Controller, Value, Channel, Port
        MIDI_remember_CC(CC_number, value1, channel, port);
        break;
      case CC_TOGGLE:
      case CC_TOGGLE_ON:
        if (first_cmd) {
          SP[Sw].State++;
          if (SP[Sw].State >= 2) SP[Sw].State = 0;
        }
        if (SP[Sw].State == 0) {
          MIDI_send_CC(CC_number, value1, channel, port); // Controller, Value, Channel, Port
          MIDI_remember_CC(CC_number, value1, channel, port);
        }
        else {
          MIDI_send_CC(CC_number, value2, channel, port); // Controller, Value, Channel, Port
          MIDI_remember_CC(CC_number, value2, channel, port);
        }
        break;
      case CC_STEP:
        // Update byte1 with the new value
        val = SP[Sw].Target_byte1;
        val += 1;
        if (val > value1) val = value2;
        SP[Sw].Target_byte1 = val;
        MIDI_send_CC(CC_number, val, channel, port); // Controller, Value, Channel, Port
        MIDI_remember_CC(CC_number, val, channel, port);
        break;
      case CC_UPDOWN:
        updown_direction_can_change = true;
        break;
    }
  }
}

// ********************************* Section 5: Page Selection Commands ********************************************

void SCO_select_page(uint8_t new_page) {
  if ( SCO_valid_page(new_page) ) {
    Previous_page = Current_page; // Store the page we come from...
    Current_page = new_page;
    prev_page_shown = 255; // Clear page number to be displayed on LEDS
    if ((Current_page != PAGE_MENU) && (Current_page != PAGE_CURRENT_DIRECT_SELECT)) {
      EEPROM.write(EEPROM_CURRENT_PAGE, Current_page);
      EEPROM.write(EEPROM_CURRENT_DEVICE, Current_device);
    }
    if (Current_page == PAGE_MENU) menu_open();
    update_page |= RELOAD_PAGE;
    update_main_lcd = true;
  }
}

bool SCO_valid_page(uint8_t page) {
  if (page < Number_of_pages) return true;
  if ((page >= FIRST_FIXED_CMD_PAGE) && (page <= LAST_FIXED_CMD_PAGE)) return true;
  return false;
}

void SCO_select_page(uint8_t new_page, uint8_t device) {
  if (device < NUMBER_OF_DEVICES) {
    Previous_device = Current_device;
    Current_device = device;
  }
  SCO_select_page(new_page);
}

void SCO_toggle_page(uint8_t page1, uint8_t page2) {
  Previous_page = Current_page; // Store the mode we come from...
  if (Current_page == page1) SCO_select_page(page2);
  else SCO_select_page(page1);
}

uint8_t current_selected_device = GP10;

void SCO_select_next_device() { // Will select the next device that is connected
  bool no_device_connected = true;
  uint8_t Tries = NUMBER_OF_DEVICES; // Limited the number of tries for the loop, in case no device is
  while (Tries > 0) {
    Tries--;
    current_selected_device++;
    DEBUGMSG("Poging:" + String(Tries) + " Device:" + String(current_selected_device));
    if (current_selected_device >= NUMBER_OF_DEVICES) current_selected_device = 0;
    if (Device[current_selected_device]->connected) { // device is selected
      Current_device = current_selected_device;
      SCO_select_page(Device[current_selected_device]->my_patch_page); // Load the patch page associated to this device
      //SCO_select_device_page(current_selected_device);
      Tries = 0; //And we are done
      no_device_connected = false;
    }
  }
  if (no_device_connected) {
    SCO_select_page(DEFAULT_PAGE);
    LCD_show_status_message("No devices...");
  }
}

void SCO_select_patch_page(uint8_t Dev) { // Will select the patch page of the current device. These can be set in init() of the device class
  if (Dev < NUMBER_OF_DEVICES) {
    Current_device = Dev;
    uint8_t new_page = Device[Dev]->my_patch_page;
    if (new_page != 0 ) SCO_select_page(new_page);
  }
}

void SCO_select_parameter_page(uint8_t Dev) { // Will select the parameter or assign page of the current device. These can be set in init() of the device class
  if (Dev < NUMBER_OF_DEVICES) {
    Current_device = Dev;
    uint8_t new_page = Device[Dev]->my_parameter_page;
    if (new_page != 0 ) SCO_select_page(new_page);
    else { // Try selecting the assign page
      new_page = Device[Dev]->my_parameter_page;
      if (new_page != 0 ) SCO_select_page(new_page);
    }
  }
}

void SCO_select_assign_page(uint8_t Dev) { // Will select the assign or patch page of the current device. These can be set in init() of the device class
  if (Dev < NUMBER_OF_DEVICES) {
    Current_device = Dev;
    uint8_t new_page = Device[Dev]->my_assign_page;
    if (new_page != 0 ) SCO_select_page(new_page);
    else { // Try selecting the patch page
      new_page = Device[Dev]->my_patch_page;
      if (new_page != 0 ) SCO_select_page(new_page);
    }
  }
}

/*void SCO_select_device_page(uint8_t device) {
  uint8_t page;
  if (device == GP10) page = PAGE_CURRENT_PATCH_BANK;
  if (device == GR55) page = PAGE_GP10_ASSIGNS;
  if (device == VG99) page = PAGE_VG99_PATCH_BANK;
  if (device == ZG3) page = PAGE_ZOOM_PATCH_BANK;
  if (device == ZMS70) page = PAGE_ZMS70_PATCH_BANK;
  DEBUGMSG("Select page: " + String(page));
  SCO_select_page(page);
}*/

// ********************************* Section 6: Global Tuner Commands ********************************************

void SCO_global_tuner_toggle() {
  if (!global_tuner_active) SCO_global_tuner_start();
  else SCO_global_tuner_stop();
}

void SCO_global_tuner_start() {
  // Start tuner or mute all devices
  DEBUGMSG("*** Activating Global tuner");
  global_tuner_active = true;
  for (uint8_t d = 0; d < NUMBER_OF_DEVICES; d++) {
    Device[d]->start_tuner();
  }
  update_main_lcd = true;
}

void SCO_global_tuner_stop() {
  // Stop tuner or unmute all devices
  global_tuner_active = false;
  for (uint8_t d = 0; d < NUMBER_OF_DEVICES; d++) {
    Device[d]->stop_tuner();
  }
  update_main_lcd = true;
}

// ********************************* Section 7: Global Tap Tempo Commands ********************************************

// Call global_tap_tempo()
// We only support bpms from 40 to 250:
#define MIN_TIME 240000 // (60.000.000 / 250 bpm)
#define MAX_TIME 1500000 // (60.000.000 / 40 bpm)

#define NUMBER_OF_TAPMEMS 5 // Tap tempo will do the average of five taps
uint32_t tap_time[NUMBER_OF_TAPMEMS];
uint8_t tap_time_index = 0;
uint32_t new_time, time_diff, avg_time;
uint32_t prev_time = 0;
bool tap_array_full = false;

void SCO_global_tap_tempo_press() {

#ifdef INTA_PIN // When using display boards, use the time from the inta interrupt, otherwise bpm timing is not correct
  new_time = time_switch_pressed;
#else
  new_time = micros(); //Store the current time
#endif
  time_diff = new_time - prev_time;
  prev_time = new_time;
  //DEBUGMSG("*** Tap no:" + String(tap_time_index) + " with difference " + String(time_diff));

  // If time difference between two taps is too short or too long, we will start new tapping sequence
  if ((time_diff < MIN_TIME) || (time_diff > MAX_TIME)) {
    tap_time_index = 1;
    tap_array_full = false;
    tap_time[0] = new_time;
    //DEBUGMSG("!!! STARTED NEW TAP SEQUENCE");
  }
  else {

    //Calculate the average time depending on if the tap_time array is full or not
    if (tap_array_full) {
      avg_time = (new_time - tap_time[tap_time_index]) / (NUMBER_OF_TAPMEMS);
    }
    else {
      avg_time = (new_time - tap_time[0]) / tap_time_index;
    }

    // Store new time in memory
    tap_time[tap_time_index] = new_time;
    //DEBUGMSG("Wrote tap_time[" + String(tap_time_index) + "] with time " + String(new_time));

    // Calculate the bpm
    Setting.Bpm = ((60000000 + (avg_time / 2)) / avg_time); // Calculate the bpm
    //EEPROM.write(EEPROM_bpm, bpm);  // Store it in EEPROM
    //DEBUGMSG("avg time:" + String(avg_time) + " => bpm: " + String(bpm));

    // Send it to the devices
    for (uint8_t d = 0; d < NUMBER_OF_DEVICES; d++) {
      Device[d]->set_bpm();
    }

    // Move to the next memory slot
    tap_time_index++;
    if (tap_time_index >= NUMBER_OF_TAPMEMS) { // We have reached the last tap memory
      tap_time_index = 0;
      tap_array_full = true; // So we need to calculate the average tap time in a different way
    }
  }
  LCD_show_status_message("Tempo " + String(Setting.Bpm) + " bpm"); // Show the tempo on the main display
  update_lcd = switch_pressed; // Update the LCD of the display above the tap tempo button
  SCO_reset_tap_tempo_LED(); // Reset the LED state, so it will flash in time with the new tempo
}

void SCO_set_global_tempo_press(uint8_t new_bpm) {
  Setting.Bpm = new_bpm;
  // Send it to the devices
  for (uint8_t d = 0; d < NUMBER_OF_DEVICES; d++) {
    Device[d]->set_bpm();
  }
  update_page = REFRESH_PAGE; //Refresh the page, so any present tap tempo button display will also be updated.
}

#define BPM_LED_ON_TIME 100 // The time the bpm LED is on in msec. 50 for real LED, 100 for virtual LED
#define BPM_LED_ADJUST 1   // LED is running a little to slow. This is an adjustment of a few msecs
uint32_t bpm_LED_timer = 0;
uint32_t bpm_LED_timer_length = BPM_LED_ON_TIME;

void SCO_update_tap_tempo_LED() {

  // Check if timer needs to be set
  if (bpm_LED_timer == 0) {
    bpm_LED_timer = millis();
  }

  // Check if timer runs out
  if (millis() - bpm_LED_timer > bpm_LED_timer_length) {
    bpm_LED_timer = millis(); // Reset the timer

    // If LED is currently on
    if (global_tap_tempo_LED == Setting.LED_bpm_colour) {
      global_tap_tempo_LED = 0;  // Turn the LED off
      bpm_LED_timer_length = (60000 / Setting.Bpm) - BPM_LED_ON_TIME - BPM_LED_ADJUST; // Set the time for the timer
    }
    else {
      global_tap_tempo_LED = Setting.LED_bpm_colour;   // Turn the LED on
      bpm_LED_timer_length = BPM_LED_ON_TIME; // Set the time for the timer
    }
    update_LEDS = true;
  }
}

void SCO_reset_tap_tempo_LED() {
  bpm_LED_timer = millis();
  global_tap_tempo_LED = Setting.LED_bpm_colour;    // Turn the LED on
  //VG99_TAP_TEMPO_LED_ON();
  bpm_LED_timer_length = BPM_LED_ON_TIME;  // Set the time for the timer
  update_LEDS = true;
}

// ********************************* Section 8: Bass Mode (Low String Priority) ********************************************

// Bass mode: sends a CC message with the number of the lowest string that is being played.
// By making smart assigns on a device, you can hear just the bass note played
uint8_t bass_string = 0; //remembers the current midi channel


// Method 1:
/*
void SCO_bass_mode_note_on(uint8_t note, uint8_t velocity, uint8_t channel, uint8_t port) {
  if ((channel >= Setting.Bass_mode_G2M_channel) && (channel <= Setting.Bass_mode_G2M_channel + 6)) {
    uint8_t string_played = channel - Setting.Bass_mode_G2M_channel + 1;
    if ((string_played == bass_string) && (velocity == 0)) bass_string = 0; // VG-99 sends NOTE ON with velocity 0 instead of NOTE OFF
    
    if ((string_played > bass_string) && (velocity >= Setting.Bass_mode_min_velocity)) {
      bass_string = string_played; //Set the bass play channel to the current channel
      if (Setting.Bass_mode_device < NUMBER_OF_DEVICES) 
        MIDI_send_CC(Setting.Bass_mode_cc_number , bass_string, Device[Setting.Bass_mode_device]->MIDI_channel, Device[Setting.Bass_mode_device]->MIDI_port);
    }
  }
}

void SCO_bass_mode_note_off(uint8_t note, uint8_t velocity, uint8_t channel, uint8_t port) {
  if ((channel >= Setting.Bass_mode_G2M_channel) && (channel <= Setting.Bass_mode_G2M_channel + 6)) {
    uint8_t string_played = channel - Setting.Bass_mode_G2M_channel + 1;
    if (string_played == bass_string) {
      bass_string = 0; //Reset the bass play channel
      //if (Setting.Bass_mode_device < NUMBER_OF_DEVICES) 
      //  MIDI_send_CC(Setting.Bass_mode_cc_number , bass_string, Device[Setting.Bass_mode_device]->MIDI_channel, Device[Setting.Bass_mode_device]->MIDI_port);
    }
  }
}
*/
// Method 2:
bool string_on[6] = { false }; // remember the current state of every string

void SCO_bass_mode_note_on(uint8_t note, uint8_t velocity, uint8_t channel, uint8_t port) {
  if ((channel >= Setting.Bass_mode_G2M_channel) && (channel <= Setting.Bass_mode_G2M_channel + 6)) {
    uint8_t string_played = channel - Setting.Bass_mode_G2M_channel;

    if (velocity >= Setting.Bass_mode_min_velocity) {
      string_on[string_played] = true;
      SCO_bass_mode_check_string();
    }
    
    else { // string level below minimum threshold or string off on VG99
      string_on[string_played] = false;
      SCO_bass_mode_check_string();
    }  
  }
}

void SCO_bass_mode_note_off(uint8_t note, uint8_t velocity, uint8_t channel, uint8_t port) {
  if ((channel >= Setting.Bass_mode_G2M_channel) && (channel <= Setting.Bass_mode_G2M_channel + 6)) {
    uint8_t string_played = channel - Setting.Bass_mode_G2M_channel;
    string_on[string_played] = false;
    SCO_bass_mode_check_string();
  }
}

void SCO_bass_mode_check_string() {
  uint8_t lowest_string_played = 0;
  for (uint8_t s = 0; s < 6; s++) { // Find the lowest string that is played (has highest string number)
    if (string_on[s]) lowest_string_played = s + 1;
  }
  if (lowest_string_played != bass_string) {
    bass_string = lowest_string_played;
    if (Setting.Bass_mode_device < NUMBER_OF_DEVICES)
      MIDI_send_CC(Setting.Bass_mode_cc_number , bass_string, Device[Setting.Bass_mode_device]->MIDI_channel, Device[Setting.Bass_mode_device]->MIDI_port);
    DEBUGMAIN("Set lowest string: " + String(bass_string));
  }
}

// ********************************* Section 9: VController Power On/Off Switching ********************************************

void SCO_switch_power_on() {
  // Switch power on
#ifdef POWER_PIN
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);
#endif
}

void SCO_switch_power_off() {
  EEP_write_eeprom_common_data(); // Save current settings
  LCD_clear_all_displays();
  LED_turn_all_off();
  LCD_show_status_message("Bye bye...");

#ifdef POWER_PIN
  // Lower the power pin
  digitalWrite(POWER_PIN, LOW);
  delay(10000); // Wait forever

#else
  // Simulate power down as there is no power pin
  // Here we start a temporary loop to emulate being switched off in case there is no poer switching
  LCD_show_status_message(""); // Clear Bye bye
  LCD_backlight_off();

  while (switch_pressed == 0) { // Wait for switch being pressed
    main_switch_check();
  }

  LCD_backlight_on();
#endif
}

bool SCO_are_you_sure() {

  switch_pressed = 0;

  // Wait for new switch to be pressed
  while (switch_pressed == 0) { // Wait for switch being pressed
    main_switch_check();

    // Update the SP.Pressed variable
    if (switch_pressed > 0) {
      SP[switch_pressed].Pressed = true;
    }
    if (switch_released > 0) {
      SP[switch_released].Pressed = false;
    }

    LED_update_pressed_state_only(); // Get the LEDs to respond
  }

  return (switch_pressed == 10);
}


