void write_control_file(void);
void write_parafile(void);
void write_current_number(int current_number);
int initialize(void);
int get_timeout(void);
int getautoflag(void);
int getecuid(char ecuid[13]);
void writesystempower(int system_p_display);
void writecurrentnumber(int current_number);
int show_data_on_lcd(int power, float energy, int count, int maxcount);