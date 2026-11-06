static void init_lcd(void)
{
   LCD_STB = 0;
   lcd_control_mode();
   lcd_wrmode();
   delay_ms(50);
   write_high_nibble(0x30);
   delay_ms(5);
   write_high_nibble(0x30);
   delay_ms(1);
   write_high_nibble(0x30);
   delay_ms(1);
   write_high_nibble(LCD_INIT4);
   delay_ms(1);
   lcd_write_command(LCD_INIT+LCD_LINES);
   lcd_write_command(DISP_CTRL+DISP_OFF);
   lcd_write_command(DISP_CLEAR);
   delay_ms(5);
   lcd_write_command(ENTRY_MODE+ADDR_INC);
   lcd_write_command(DISP_CTRL+DISP_ON);
   delay_ms(5);
}
