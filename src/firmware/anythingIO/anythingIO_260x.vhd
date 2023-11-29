--LIBRARY ieee;
--USE ieee.std_logic_1164.ALL;

LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.numeric_std.ALL;
USE ieee.std_logic_arith.ALL;
USE ieee.std_logic_unsigned.ALL;

entity PC104 is port
(
-- bus interface signals --
--  BALE: in STD_LOGIC; 
--  TC: in STD_LOGIC; 
--  BOSC: in STD_LOGIC; 
--  RSTDRV: in STD_LOGIC; 	   --System reset
--  REFRES: in STD_LOGIC; 
--  SBHE: in STD_LOGIC; 
--  MASTER: in STD_LOGIC; 
--  SMEMR: in STD_LOGIC;   
--  SMEMW: in STD_LOGIC;  
--  IOCHRDY: inout STD_LOGIC; 
--  MEMCS16: inout STD_LOGIC; 
  IOCS16: inout STD_LOGIC; 
--  IRQ: out STD_LOGIC_VECTOR (15 downto 3); 		-- interrupts
--  DACK: in STD_LOGIC_VECTOR (7 downto 6); 		-- dma acknowledges
--  DRQ: out STD_LOGIC_VECTOR (7 downto 6); 		-- dma requests
  LBE: out STD_LOGIC;
  LBDIR: out STD_LOGIC;
  SYSCLK: in STD_LOGIC;  --PC104 bus clock PIN 185 GCK3
  DIS: out STD_LOGIC;
  AEN: in STD_LOGIC;
  IORC: in STD_LOGIC;   
  IOWC: in STD_LOGIC;
  SA: in STD_LOGIC_VECTOR (9 downto 0); 		-- address bus
  SD: out STD_LOGIC_VECTOR (15 downto 0); 		-- data bus

-- System Clock, reset
  SynClk: in STD_LOGIC;  --50 MHz local clock PIN 80 GCK0

-- Application Interfaces: CN Counter
  pulse0: in std_logic;
  pulse1: in std_logic;
  latch_count: in std_logic;
-- Application Interfaces: AN-232 Radar Altimeter
  alt_data: in std_logic;
  alt_clock: in std_logic;
  alt_data_tp: out std_logic;
  alt_clock_tp: out std_logic;
-- Application Interfaces: Debug
  LEDS: out STD_LOGIC_VECTOR(7 downto 0);
  debug_out: out std_logic;
  ldebug: out std_logic;
  pdebug: out std_logic;
  ndebug: out std_logic;
  cdebug: out std_logic
);
end PC104;
Architecture behavior of PC104 is
-- 0x220 hex  
  constant Decode : STD_LOGIC_VECTOR (5 downto 0) := "100010"; 
  
-- misc global signals --
  signal IORC_buff: std_logic:='1';
  signal CardSelect: STD_LOGIC:='0';
  signal WordAccess: STD_LOGIC;
  signal WordSel: STD_LOGIC_VECTOR(7 downto 0);	
  signal DBus: STD_LOGIC_VECTOR (15 downto 0);
  
-- Application Signals: CN Counter
  signal pulse0_old1: std_logic:='0';                              -- Pulse input0 from CN counter, 1x registered
  signal pulse1_old1: std_logic:='0';                              -- Pulse input1 from CN counter, 1x registered
  signal pulse0_old2: std_logic:='0';                              -- Pulse input0 from CN counter, 2x registered
  signal pulse1_old2: std_logic:='0';                              -- Pulse input1 from CN counter, 2x registered
  signal latch_count_old1: std_logic:='0';                         -- Count latch input signal, 1x registered
  signal latch_count_old2: std_logic:='0';                         -- Count latch input signal, 2x registered
  signal count0: std_logic_vector(15 downto 0):=X"0000";           -- Pulse counter/accumulator for input0
  signal count1: std_logic_vector(15 downto 0):=X"0000";           -- Pulse counter/accumulator for input1
  signal count0_reg: std_logic_vector(15 downto 0):=X"0000";       -- Pulse count from input0, registered for data bus
  signal count1_reg: std_logic_vector(15 downto 0):=X"0000";       -- Pulse count from input1, registered for data bus
  signal count_clear: std_logic:='0';

-- Application Signals: Radar Altimeter
  signal altitude: std_logic_vector(23 downto 0):=X"000000";       -- altitude word passed to the PC104 data bus
  signal altitude_sreg: std_logic_vector(23 downto 0):=X"000000";  -- input altitude value from altimeter, registered
  signal alt_data_old1: std_logic:='0';                            -- input data word from altimeter, 1x registered
  signal alt_data_old2: std_logic:='0';                            -- input data word from altimeter, 2x registered
  signal alt_clock_old1: std_logic:='0';                           -- input clock from altimeter, 1x registered
  signal alt_clock_old2: std_logic:='0';                           -- input clock from altimeter, 2x registered
  signal alt_clock_state: std_logic:='0';                          -- input clock from altimeter, digitally filtered
  signal alt_clock_state_d: std_logic:='0';                        -- input clock from altimeter, digitally filtered delayed
  signal alt_clock_redge: std_logic:='0';                          -- alt_clock_state rising edge 
  signal alt_clock_state_cntr: std_logic_vector (15 downto 0) :=X"0000";  -- input clock digital filter counter
  signal timeout_count: STD_LOGIC_VECTOR (23 downto 0):=X"000000"; -- Data receiver state machine timeout counter
  signal bit_count: STD_LOGIC_VECTOR (4 downto 0):="00000";        -- Data receiver bit counter
  type t_alt_rx_state is (NOCLK, IDLE, SYNCNEXT, RECEIVING, COMPLETE);
  signal alt_rx_state: t_alt_rx_state := NOCLK;                    -- Data receiver state
  signal alt_tp_counter: STD_LOGIC_VECTOR (23 downto 0):=X"000000";-- Test Pattern counter, used for state timing
  signal alt_tp_data: STD_LOGIC:='0';                              -- Test Pattern output data bit
  signal alt_tp_clk: STD_LOGIC:='0';                               -- Test Pattern output clock bit
  signal alt_tp_word:  STD_LOGIC_VECTOR(23 downto 0):="000000000000000000000000";
  signal alt_tp_word1: STD_LOGIC_VECTOR(23 downto 0):="100000000100100111110001"; -- Example Word from Altimeter (294 ft with bit0 asserted --> SD=295,0x127 expected).
  signal alt_tp_word2: STD_LOGIC_VECTOR(23 downto 0):="000000000100101000010001"; -- Example Word from Altimeter (296 ft with bit0 asserted --> SD=297,0x129 expected).
  signal alt_tp_word_hold_cntr: STD_LOGIC_VECTOR(3 downto 0):="0000";

-- Application Signals: Debug
  signal ledcount: STD_LOGIC_VECTOR (31 downto 0) := X"00000000";

begin

-- Bus Signal Constants
--    DRQ <= "ZZ";
--    IRQ <= "ZZZZZZZZZZZZZ";
    DIS <= '0';		-- uncomment to leave configuration decode on
--    DIS <= '1';		-- uncomment to disable configuration decode
--    MEMCS16 <= 'Z';
--    IOCHRDY <= 'Z';


  -------------------------------------------------------------------------------------------------------------------
  ---------------------------------------- AN-232 Radar Altimeter Application ---------------------------------------
  -------------------------------------------------------------------------------------------------------------------
  -- HGM232 ALT DIGITAL output Clock and data comes in 24-bit word (~2ms total) followed by an idle period
  -- Data is captured on the rising edge clock. Clk idles at high state.
  -- Data Receiver has 4 states, alt_rx_state: (0) NOCLK (1) IDLE (2) SYNCNEXT (3) RECEIVING (4) COMPLETE
  --    During normal operation, states cycle Idle->SyncNext->Receiving->Complete->Idle
  --    and NOCLK is entered only if there is a timeout in expected state-flow behavior
  -- Asynchronous Clock and data signal inputs are double-buffered to reduce meta-instability
  -- After >2.6ms (2^18/50MHz) in the idle phase, state advances to SyncNext and is ready to receive 1st bit.
  -- When first bit is received on alt_clk rising edge, advance to Receiving state.
  -- When last bit is received, advance to Complete state for one cycle to register the value and then clear.
  -- If data has not been received for ~80ms (2^22/50MHz), then state goes to NOCLK and the altitude output is 0xFFFFF
  -- To reduce susceptibility to clock edge noise, the alt_clock input is double-buffered and verified X cycles 
  --    before changing alt_clock_state. This achieves a digital filter of the clock to reject noise.

  -- Radar Altimeter Decoding, 24-bit word from altimeter is condensed into 16-bit (with altitude value and status)
  --  24-bit word from Instrument
  --  0: =1
  --  1: Transceiver FAIL
  --  2: Search or Blanking
  --  3: Test in Progress
  --  4: Warning monitor
  --  5: 1/2 ft. altitude
  --  6: 1   ft. altitude
  --  ...
  --  20: 2^14 ft. altitude
  --  21: 2^15 ft. altitude
  --  22: Negative altitude sign
  --  23: Odd Partiy Bit
  
  RadarAltimeterBuffers: process (SynClk)
  begin
    if rising_edge(SynClk) then
      alt_clock_old2 <= alt_clock_old1;      
      alt_clock_old1 <= alt_clock;
      alt_data_old2 <= alt_data_old1;
      alt_data_old1 <= alt_data;
    end if;
  end process RadarAltimeterBuffers;

  RadarAltimeterClockState: process (SynClk)
  begin
    if rising_edge(SynClk) then
	 
      -- alt_clock_state_cntr increments when incoming state is different than alt_clock_state
      --     Indicating that the incoming clock has transitioned (or noise)
      if (alt_clock_state = '1') and (alt_clock_old2 = '0') then
        alt_clock_state_cntr <= alt_clock_state_cntr + 1;
      elsif (alt_clock_state = '0') and (alt_clock_old2 = '1') then
        alt_clock_state_cntr <= alt_clock_state_cntr + 1;
      else  -- Unknown state
        alt_clock_state_cntr <= X"0000";
      end if;

      -- alt_clock_state transitions after stable for 512 Sysclk cycles (512/50e6~10us)
      --     Note that the bit period is ~75us, so delaying rising edge data sampling by 10us is OK.
      if ((alt_clock_state = '1') or (alt_clock_state = '0')) and (alt_clock_state_cntr = X"0200") then
        alt_clock_state <= not alt_clock_state;
      elsif (alt_clock_state = '1') or (alt_clock_state = '0') then
        alt_clock_state <= alt_clock_state;
      else  -- Unknown state
        alt_clock_state <= '0';
      end if;

      -- Clock state rising edge flag, single cycle
      alt_clock_state_d <= alt_clock_state;
      if (alt_clock_state_d = '0') and (alt_clock_state = '1') then
        alt_clock_redge <= '1';
      else
        alt_clock_redge <= '0';
      end if;
		
    end if;
  end process RadarAltimeterClockState;

  RadarAltimeterStateMachine: process (SynClk)
  begin
    if rising_edge(SynClk) then
      if    (alt_rx_state = NOCLK) then
        if (alt_clock_redge = '1') then
          alt_rx_state <= IDLE;
		  else
          alt_rx_state <= NOCLK;
        end if;
      elsif (alt_rx_state = IDLE) then
        if (timeout_count(17) = '1') then
          alt_rx_state <= SYNCNEXT;
        elsif (timeout_count(22) = '1') then
          alt_rx_state <= NOCLK;
        else
          alt_rx_state <= IDLE;
        end if;
      elsif (alt_rx_state = SYNCNEXT) then
        if (alt_clock_redge = '1') then
          alt_rx_state <= RECEIVING;
        elsif (timeout_count(22) = '1') then
          alt_rx_state <= NOCLK;
        else
          alt_rx_state <= SYNCNEXT;
        end if;
      elsif (alt_rx_state = RECEIVING) then
        if (bit_count = "11000") then
          alt_rx_state <= COMPLETE;
        elsif (timeout_count(22) = '1') then
          alt_rx_state <= NOCLK;
        else
          alt_rx_state <= RECEIVING;
        end if;
      elsif (alt_rx_state = COMPLETE) then
        alt_rx_state <= IDLE;
      else
        alt_rx_state <= NOCLK;   -- In unknown state, return to NOCLK
      end if;

    end if;
  end process RadarAltimeterStateMachine;

  RadarAltimeterTimeoutCounter: process (SynClk)
  begin
    if rising_edge(SynClk) then
      if (alt_rx_state = COMPLETE) or (alt_rx_state = NOCLK) then
        timeout_count <= X"000000";
      elsif (alt_rx_state = IDLE) then
        if (alt_clock_state = '0') then -- IDLE should not advance to SYNCNEXT if clock fails to remain in idle state (=1)
          timeout_count <= X"000000";
        else
          timeout_count <= timeout_count + 1;
        end if;
      elsif (alt_rx_state = SYNCNEXT) or (alt_rx_state = RECEIVING) then
        timeout_count <= timeout_count + 1;
      else
        timeout_count <= X"000000";
      end if;

    end if;
  end process RadarAltimeterTimeoutCounter;

  -- Digital data is right shifted into shift-register, and then register-transferred after the 24th bit is received.
  RadarAltimeterShiftReg: process (SynClk)
  begin
    if rising_edge(SynClk) then
      if (alt_rx_state = SYNCNEXT) and (alt_clock_redge = '1') then
        bit_count <= "00001";
        altitude_sreg(23) <= alt_data_old2;
        altitude_sreg(22 downto 0) <= "00000000000000000000000";
      elsif (alt_rx_state = RECEIVING) and (alt_clock_redge = '1') then
        bit_count <= bit_count + 1;
        altitude_sreg(23 downto 0) <= alt_data_old2 & altitude_sreg(23 downto 1) ; --Right shift bits into register
      elsif (alt_rx_state = RECEIVING) then  -- Hold values while receiving
        bit_count <= bit_count;
        altitude_sreg <= altitude_sreg;
      elsif (alt_rx_state = COMPLETE) or (alt_rx_state = NOCLK) or (alt_rx_state = IDLE) then
        bit_count <= "00000";
        altitude_sreg(23 downto 0) <= "000000000000000000000000";
      else                                -- Unknown condition, clear values.
        bit_count <= "00000";
        altitude_sreg(23 downto 0) <= "000000000000000000000000";
      end if;

      -- When word reception is complete, register the word but modify if various status events occur
      --     Don't update data (hold value) if received "500ft test in progress (AST)" word
      --     Replace altitude bits with FAIL altitude code if FAIL bit is asserted
      --     Replace altitude bits with SEARCH altitude code if SEARCH bit is asserted
      if (alt_rx_state = COMPLETE) then
        if (altitude_sreg(3) = '1'   ) then
          altitude <= altitude;
        elsif (altitude_sreg(1) = '1') then
          altitude(23 downto 22) <= altitude_sreg(23 downto 22);
          altitude(21 downto  5) <= "01111111111111110"; -- FAIL altitude code = 32,767ft. 
          altitude( 4 downto  0) <= altitude_sreg(4 downto 0);
        elsif (altitude_sreg(2) = '1') then
          altitude(23 downto 22) <= altitude_sreg(23 downto 22);
          altitude(21 downto  5) <= "01111111000000000"; -- SEARCH altitude code = 32,512ft.
          altitude( 4 downto  0) <= altitude_sreg(4 downto 0);
        else
          altitude <= altitude_sreg;   -- Update the altitude word
        end if;
      elsif (alt_rx_state = IDLE) or (alt_rx_state = SYNCNEXT) or (alt_rx_state = RECEIVING) then
        altitude <= altitude;
      else -- includes NOCLK state
        altitude <= X"EEEEEE";  -- Note: only a portion of this word is eventually transferred to data bus
      end if;

    end if;
  end process RadarAltimeterShiftReg;
  
  -- Output Altimeter Clock test pattern
  --     Actual clock has ~75us bit period. 
  --     Test pattern has (2^12/50MHz=82us) bit period and (2^19/50MHz=10.5ms) pattern repeat
  RadarAltimeterPatternCounter: process (SynClk)
  begin
    if rising_edge(SynClk) then
      if (alt_tp_counter(19) = '1') then
        alt_tp_counter <= X"000000";
        alt_tp_word_hold_cntr <= alt_tp_word_hold_cntr + 1;
      else
        alt_tp_counter <= alt_tp_counter + 1;
        alt_tp_word_hold_cntr <= alt_tp_word_hold_cntr;
      end if;
    end if;
  end process RadarAltimeterPatternCounter;

  RadarAltimeterPatternClock: process (SynClk)
  begin
    if rising_edge(SynClk) then
      alt_clock_tp <= alt_tp_clk;

      -- Clock rising edge is aligned to data bit center
      if (alt_tp_counter(11) = '0') and (alt_tp_counter(23 downto 12) < X"018" ) then
        alt_tp_clk <= '0';
      else
        alt_tp_clk <= '1';
      end if;
    end if;
  end process RadarAltimeterPatternClock;

  RadarAltimeterPatternData: process (SynClk)
  begin
    if rising_edge(SynClk) then
		
      -- Ping-pong the test pattern word
      --   Note: the word change-out must be matched to the moment of the counter reset
      if    (alt_tp_counter(19) = '1') and (alt_tp_word_hold_cntr = "0000") then
        alt_tp_word <= alt_tp_word1; -- switch to next word
      elsif (alt_tp_counter(19) = '1') and (alt_tp_word_hold_cntr = "1000") then
        alt_tp_word <= alt_tp_word2; -- switch to next word
      else
        alt_tp_word <= alt_tp_word;  -- hold word until finished
      end if;
	 
      if    (alt_tp_counter(23 downto 12) = X"000" ) then
        alt_tp_data <= alt_tp_word(0);
      elsif (alt_tp_counter(23 downto 12) = X"001" ) then
        alt_tp_data <= alt_tp_word(1);
      elsif (alt_tp_counter(23 downto 12) = X"002" ) then
        alt_tp_data <= alt_tp_word(2);
      elsif (alt_tp_counter(23 downto 12) = X"003" ) then
        alt_tp_data <= alt_tp_word(3);
      elsif (alt_tp_counter(23 downto 12) = X"004" ) then
        alt_tp_data <= alt_tp_word(4);
      elsif (alt_tp_counter(23 downto 12) = X"005" ) then
        alt_tp_data <= alt_tp_word(5);
      elsif (alt_tp_counter(23 downto 12) = X"006" ) then
        alt_tp_data <= alt_tp_word(6);
      elsif (alt_tp_counter(23 downto 12) = X"007" ) then
        alt_tp_data <= alt_tp_word(7);
      elsif (alt_tp_counter(23 downto 12) = X"008" ) then
        alt_tp_data <= alt_tp_word(8);
      elsif (alt_tp_counter(23 downto 12) = X"009" ) then
        alt_tp_data <= alt_tp_word(9);
      elsif (alt_tp_counter(23 downto 12) = X"00a" ) then
        alt_tp_data <= alt_tp_word(10);
      elsif (alt_tp_counter(23 downto 12) = X"00b" ) then
        alt_tp_data <= alt_tp_word(11);
      elsif (alt_tp_counter(23 downto 12) = X"00c" ) then
        alt_tp_data <= alt_tp_word(12);
      elsif (alt_tp_counter(23 downto 12) = X"00d" ) then
        alt_tp_data <= alt_tp_word(13);
      elsif (alt_tp_counter(23 downto 12) = X"00e" ) then
        alt_tp_data <= alt_tp_word(14);
      elsif (alt_tp_counter(23 downto 12) = X"00f" ) then
        alt_tp_data <= alt_tp_word(15);
      elsif (alt_tp_counter(23 downto 12) = X"010" ) then
        alt_tp_data <= alt_tp_word(16);
      elsif (alt_tp_counter(23 downto 12) = X"011" ) then
        alt_tp_data <= alt_tp_word(17);
      elsif (alt_tp_counter(23 downto 12) = X"012" ) then
        alt_tp_data <= alt_tp_word(18);
      elsif (alt_tp_counter(23 downto 12) = X"013" ) then
        alt_tp_data <= alt_tp_word(19);
      elsif (alt_tp_counter(23 downto 12) = X"014" ) then
        alt_tp_data <= alt_tp_word(20);
      elsif (alt_tp_counter(23 downto 12) = X"015" ) then
        alt_tp_data <= alt_tp_word(21);
      elsif (alt_tp_counter(23 downto 12) = X"016" ) then
        alt_tp_data <= alt_tp_word(22);
      elsif (alt_tp_counter(23 downto 12) = X"017" ) then
        alt_tp_data <= alt_tp_word(23);
      else
        alt_tp_data <= '0';
      end if;

      alt_data_tp <= alt_tp_data; -- Put pattern on output port

    end if;
  end process RadarAltimeterPatternData;



  -------------------------------------------------------------------------------------------------------------------
  -------------------------------------------------------------------------------------------------------------------

  -------------------------------------------------------------------------------------------------------------------
  ---------------------------------------- CN Counter Application ---------------------------------------------------
  -------------------------------------------------------------------------------------------------------------------
  -- Pulses accumulations are cleared and initiated when the latch signal rising edge is received (latch_count, 100Hz external)
  -- Incoming pulses are edge sampled by the system clock (SysClk, 50MHz).
  
  PulseBuffers: process (SynClk)
  begin
    if rising_edge(SynClk) then
      pulse0_old1 <= pulse0;
      pulse0_old2 <= pulse0_old1;
      pulse1_old1 <= pulse1;
      pulse1_old2 <= pulse1_old1;
      latch_count_old1 <= latch_count;	 
      latch_count_old2 <= latch_count_old1;
    end if;
  end process PulseBuffers;

  PulseCounters: process (SynClk)
  begin
    if rising_edge(SynClk) then
      if (count_clear = '1') then
        count0 <= X"0000";
        count1 <= X"0000";
      else

        if (pulse0_old2 = '0') and (pulse0_old1 = '1') then
          count0 <= count0 + 1;
        else
          count0 <= count0;		
        end if;
		  
        if (pulse1_old2 = '0') and (pulse1_old1 = '1') then
          count1 <= count1 + 1;
        else
          count1 <= count1;
        end if;
      end if;
    end if;
  end process PulseCounters;

  LatchCounts: process (SynCLk)
  begin
    if rising_edge(SynClk) then
      if (latch_count_old2 = '0') and (latch_count_old1 = '1') then
        count_clear <= '1';
        count0_reg <= count0;	   
        count1_reg <= count1;
      else 
        count_clear <= '0';
        count0_reg <= count0_reg;	   
        count1_reg <= count1_reg;
      end if;
    end if;
  end process LatchCounts;
  -------------------------------------------------------------------------------------------------------------------
  -------------------------------------------------------------------------------------------------------------------

  -------------------------------------------------------------------------------------------------------------------
  ------------------------------------ Debug Signal, LED Blinker Application ----------------------------------------
  -------------------------------------------------------------------------------------------------------------------
  DebugSignal: process (SynCLk)
  begin
    if rising_edge(SynClk) then
      cdebug <= count_clear;
      pdebug <= count0(0) or count1(0);
      ndebug <= alt_data_old2;

      if (alt_rx_state = SYNCNEXT) then
        ldebug <= '1';
      else
        ldebug <= '0';
      end if;

      -- Options for outputting a debug signal that comes out on the Mesa P2 connector and through to the DSM faceplate.
      debug_out <= 'Z'; 
--      debug_out <= alt_clock_state;
--      debug_out <= timeout_count(17);
--      debug_out <= alt_data_old2;
--      if (alt_rx_state = RECEIVING) then
--        debug_out <= '1';
--      else
--        debug_out <= '0';
--      end if;

      -- Note: LEDs on the card have inverted logic. Setting =0 turns the LED ON
      leds(0) <= not ledcount(23);
      leds(1) <= not ledcount(24);
      leds(2) <= not ledcount(25);
      leds(3) <= not ledcount(26);
      leds(4) <= not ledcount(27);	
      leds(5) <= not ledcount(28);

      -- LED is OFF if there are no input pulses
      if (count0 = X"0000") and (count1 = X"0000") then 
        leds(6) <= '1';
      else
        leds(6) <= '0';
      end if;

      -- LED is OFF if there is no data being received
      if (alt_rx_state = NOCLK) then
        leds(7) <= '1';
      else
        leds(7) <= '0';
      end if;

    end if;
  end process DebugSignal;

  ledblinker: process (SynClk) 
  begin
    if rising_edge(SynClk) then
      ledcount <= ledcount + 1;			
    end if;
  end process ledblinker;
  -------------------------------------------------------------------------------------------------------------------
  -------------------------------------------------------------------------------------------------------------------

------ Original Code for Bus control
--  CSelect: process (AEN)
--  begin 
--    if SA(9 downto 4) = Decode and AEN = '0' then 
--      CardSelect <= '1';
--      LBE <= '0';
--    else
--      CardSelect <= '0';
--      LBE <= '1';
--    end if;
--  end process CSelect;	
--  
--  Localbuffer: process (CardSelect, IORC)
--  begin
--    if (CardSelect = '1') and (IORC = '0') then
--      LBDIR <= '0';
--    else
--      LBDIR <= '1';
--    end if;
--  end process Localbuffer;	
--
--  AddDecode: process (SA, AEN, IORC)
--  begin
--    if SA(3 downto 0) = "0010" and SA(9 downto 4) = Decode and AEN = '0' then
--      IOCS16 <= '0';
--      if IORC = '0' then 
--      end if;		
--    elsif SA(3 downto 0) = "0011" and SA(9 downto 4) = Decode and AEN = '0' then 
--      IOCS16 <= 'Z';	       
--      if IORC = '0' then 
--      end if;     
--    elsif SA(3 downto 0) = "0100" and SA(9 downto 4) = Decode and AEN = '0' then       
--      IOCS16 <= '0';
--      if IORC = '0' then 
--      end if;     
--    elsif SA(3 downto 0) = "0101" and SA(9 downto 4) = Decode and AEN = '0' then
--      IOCS16 <= 'Z';
--    elsif SA(3 downto 0) = "0110" and SA(9 downto 4) = Decode and AEN = '0' then
--      IOCS16 <= '0';
--      if IORC = '0' then       
--      end if;     
--    elsif SA(3 downto 0) = "0111" and SA(9 downto 4) = Decode and AEN = '0' then
--      IOCS16 <= 'Z';
--    elsif SA(3 downto 0) = "1000" and SA(9 downto 4) = Decode and AEN = '0' then 
--      IOCS16 <= '0';
--      if IORC = '0' then 
--        SD(15 downto 0) <= count0_reg(15 downto 0);
--      end if;
--    elsif SA(3 downto 0) = "1010" and SA(9 downto 4) = Decode and AEN = '0' then 
--      IOCS16 <= '0';
--      if IORC = '0' then 
--        SD(15 downto 0) <= count1_reg(15 downto 0); 
--      end if;	       
--    elsif SA(3 downto 0) = "1100" and SA(9 downto 4) = Decode and AEN = '0' then 
--      IOCS16 <= '0';
--      if IORC = '0' then 
--        SD(15 downto 8) <= altitude(21 downto 14);      
--        SD(7 downto 1) <= altitude(13 downto 7);      
--        SD(0) <= altitude(1) or altitude(2) or altitude (4);
--      end if;
--    else
--      SD(15 downto 0) <= "ZZZZZZZZZZZZZZZZ";
--      IOCS16 <= 'Z';
--    end if;
--  end process AddDecode;

-- New Implementation based on Mesa 4i34 example code
  CSelect: process (SA, AEN, CardSelect) 
  begin 
    if SA(9 downto 4) = Decode and AEN = '0' then 
      CardSelect <= '1';
    else
      CardSelect <= '0';
    end if;		

    WordAccess <= CardSelect; 				

    if SA(3 downto 0) = "0000" and CardSelect = '1' then 
      WordSel(0) <= '1'; 
    else 
      WordSel(0) <= '0';
    end if;

    if SA(3 downto 0) = "0010" and CardSelect = '1' then 
      WordSel(1) <= '1'; 
    else 
      WordSel(1) <= '0';
    end if;
		
    if SA(3 downto 0) = "0100" and CardSelect = '1' then 
      WordSel(2) <= '1'; 
    else 
      WordSel(2) <= '0';
    end if;

    if SA(3 downto 0) = "0110" and CardSelect = '1' then 
      WordSel(3) <= '1'; 
    else 
      WordSel(3) <= '0';
    end if;	

    if SA(3 downto 0) = "1000" and CardSelect = '1' then 
      WordSel(4) <= '1'; 
    else 
      WordSel(4) <= '0';
    end if;

    if SA(3 downto 0) = "1010" and CardSelect = '1' then 
      WordSel(5) <= '1'; 
    else 
      WordSel(5) <= '0';
    end if;

    if SA(3 downto 0) = "1100" and CardSelect = '1' then 
      WordSel(6) <= '1'; 
    else 
      WordSel(6) <= '0';
    end if;

    if SA(3 downto 0) = "1110" and CardSelect = '1' then 
      WordSel(7) <= '1'; 
    else 
      WordSel(7) <= '0';
    end if;	
  end process;

  GenIOCS: process (CardSelect, WordAccess)
  begin
    if (CardSelect = '1') and WordAccess = '1' then  	-- address 4 thru 7 are 16 bit
      IOCS16 <= '0';
    else
      IOCS16 <= 'Z';
    end if;
  end process GenIOCS;	


  PortDecode: process (SynClk,WordSel,IORC_buff,IOWC)
  begin
    if rising_edge(SynClk) then
      if    (WordSel(0) = '1') and (IORC_buff = '0') then
        DBus <= X"0000";
      elsif (WordSel(1) = '1') and (IORC_buff = '0') then
        DBus <= X"1111";
      elsif (WordSel(2) = '1') and (IORC_buff = '0') then
        DBus <= X"2222";
      elsif (WordSel(3) = '1') and (IORC_buff = '0') then
        DBus <= X"3333";
      elsif (WordSel(4) = '1') and (IORC_buff = '0') then
        DBus <= count0_reg;
      elsif (WordSel(5) = '1') and (IORC_buff = '0') then
        DBus <= count1_reg;
      elsif (WordSel(6) = '1') and (IORC_buff = '0') then
        ---------- 50kft range, 2ft resolution, status bit[0]= (FAIL || SEARCH || WARNING) ----------------
        --DBus(15 downto 1) <= altitude(21 downto 7);
        --DBus(0)           <= altitude(1) or altitude(2) or altitude (4);
        ---------------------------------------------------------------------------------------------------
        ---------- 32kft range, 1ft resolution, status bit[15]= (FAIL || SEARCH || WARNING) ----------------
        --DBus(14 downto 0) <= altitude(20 downto 6);
        --DBus(15)           <= altitude(1) or altitude(2) or altitude (4);
        ---------------------------------------------------------------------------------------------------
        ---------- 32kft range, 2ft resolution, status bit[15]=FAIL, status bit[0]=(SEARCH || WARNING) ----
        --- NOCLK case passes DBus <= 0xBBBB (48,059 ft.)
        DBus(15)          <= altitude(1);
        DBus(14 downto 1) <= altitude(20 downto 7);
        DBus(0)           <= altitude(2) or altitude (4);
        ----------------------------------------------------------------------------------------------------
      elsif (WordSel(7) = '1') and (IORC = '0') then
        DBus <= X"7777";
      else 
        DBus <= X"FFFF";
      end if;
    end if;
  end process PortDecode;

  BuffIORC: process (SYSCLK) begin
    if rising_edge(SYSCLK) then
      IORC_buff <= IORC;
    end if;
  end process BuffIORC;

  localbuffer: process (CardSelect, IORC_buff) begin	
    if CardSelect = '1' then 
      LBE <= '0'; 
    else 
      LBE <= '1'; 
    end if;
		
    if (CardSelect = '1') and (IORC_buff = '0') then
      LBDIR <= '0';
    else 
      LBDIR <= '1'; 
    end if;
  end process;

  SDDrivers: process (CardSelect, WordAccess, DBus, IORC_buff)
  begin 
    if (CardSelect = '1') and (IORC_buff) = '0' then
      if WordAccess = '1' then
        SD <= DBus;
      else 
  	     SD(7 downto 0) <= DBus(7 downto 0);
  	     SD(15 downto 8) <= "ZZZZZZZZ";
  	   end if;
  	 else
  	   SD <= "ZZZZZZZZZZZZZZZZ";			
  	 end if;
  end process SDDrivers;

end behavior;


