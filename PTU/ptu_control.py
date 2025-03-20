import time
import serial
import sys
import csv
import signal
import termios
from pynput import keyboard
from prompt_toolkit import prompt

#######################################
# Global Variables & Configuration   #
#######################################

ser = None
serial_port = '/dev/ttyUSB0'
pressed_keys = set()
address = 0x01
running = True
listener = None

# Speeds for manual pan/tilt via arrow keys (0x00–0x3F)
PAN_SPEED = 0x50
TILT_SPEED = 0x50

# Pelco-D bits
BIT_LEFT  = 0x04
BIT_RIGHT = 0x02
BIT_UP    = 0x08
BIT_DOWN  = 0x10

# Dictionary to store software-based presets:
#   presets[preset_num] = (pan_msb, pan_lsb, tilt_msb, tilt_lsb)
presets = {}

def graceful_exit(signum, frame):
    global running, ser, list_v2ener
    print("\nCtrl+C detected.")
    running = False
    try:
        if listener is not None:
            listener.stop()
    except Exception as e:
        print("Error stopping listener:", e)
    try:
        if ser is not None and ser.is_open:
            ser.close()
    except Exception as e:
        print("Error closing serial port:", e)
    sys.exit(0)

# Register the signal handler for SIGINT.
signal.signal(signal.SIGINT, graceful_exit)

#######################################
#     Terminal Echo Disable Setup     #
#######################################

# Save original terminal settings so we can restore later.
fd = sys.stdin.fileno()
original_term = termios.tcgetattr(fd)
new_term = termios.tcgetattr(fd)
new_term[3] = new_term[3] & ~termios.ECHO  # lflags: disable ECHO
termios.tcsetattr(fd, termios.TCSADRAIN, new_term)

#######################################
#   Query / Set Position Commands     #
#######################################

def send_command_and_read_response(cmd_bits, msb=0, lsb=0):
    send_pelco_d_command(cmd_bits, msb, lsb)
    response = ser.read(7)
    if len(response) < 7:
        print("Warning: Incomplete response or timeout.")
        return None
    return response

def query_pan_position():
    resp = send_command_and_read_response(cmd_bits=0x51, msb=0x00, lsb=0x00)
    if resp and len(resp) == 7 and resp[3] == 0x59:
        return (resp[4], resp[5])
    return (None, None)

def query_tilt_position():
    resp = send_command_and_read_response(cmd_bits=0x53, msb=0x00, lsb=0x00)
    if resp and len(resp) == 7 and resp[3] == 0x5B:
        return (resp[4], resp[5])
    return (None, None)

def set_pan_position(msb, lsb):
    send_pelco_d_command(0x4B, msb, lsb)
    print(f"Set pan position -> {msb:02X} {lsb:02X}")


def set_tilt_position(msb, lsb):
    send_pelco_d_command(0x4D, msb, lsb)
    print(f"Set tilt position -> {msb:02X} {lsb:02X}")

def set_positioning_speed():
    send_pelco_d_command(0x5F, PAN_SPEED, TILT_SPEED)
    print("Set positioning speed")

def self_check():
    send_pelco_d_command(0x07, 0x00, 0x77)
    print("Self-checking")

def stop_motion():
    send_pelco_d_command(0x00, 0x00, 0x00)

def calculate_angles(pan_pos, tilt_pos):
    if pan_pos[0] is None or tilt_pos[0] is None:
            print("Error: Could not read current pan/tilt.")
            return
    pan_angle = (pan_pos[0] * 256 + pan_pos[1]) / 100
    tilt_angle = (tilt_pos[0] * 256 + tilt_pos[1])
    if(tilt_angle > 18000):
        tilt_angle = (tilt_angle - 36000)
    tilt_angle = tilt_angle / 100

    return (pan_angle, tilt_angle)

def get_angles():
    pan_pos = query_pan_position()
    tilt_pos = query_tilt_position()

    pan_angle, tilt_angle = calculate_angles(pan_pos, tilt_pos)
    print(f"Pan angle: {pan_angle}° Tilt angle: {tilt_angle}°")

    return (pan_angle, tilt_angle)

def go_home():
    global PAN_SPEED, TILT_SPEED
    old_p_spd = PAN_SPEED
    old_t_spd = TILT_SPEED
    PAN_SPEED = 63
    TILT_SPEED = 63
    bits, p_spd, t_spd = get_current_bits_and_speeds()
    if bits != 0:
        send_pelco_d_command(bits, p_spd, t_spd)
    pan_pos = [0x41, 0x2D]
    tilt_pos = [0x8C, 0xA0]
    set_positioning_speed()
    set_pan_position(pan_pos[0], pan_pos[1])
    set_tilt_position(tilt_pos[0],tilt_pos[1])
    pan_angle, tilt_angle = calculate_angles(pan_pos, tilt_pos)
    PAN_SPEED = old_p_spd
    TILT_SPEED = old_t_spd
    bits, p_spd, t_spd = get_current_bits_and_speeds()
    if bits != 0:
        send_pelco_d_command(bits, p_spd, t_spd)
    print(f"Pan angle: {pan_angle}° Tilt angle: {tilt_angle}°")
    
def log_angles_to_csv(t_pan_angle, t_tilt_angle, hz=10, delta=0.01):
    """
    Logs current pan/tilt angles at 10 Hz with a timestamp into angles_log.csv.
    The logging runs for the specified duration (in seconds).
    """
    timestamp = time.strftime("%Y-%m-%d_%H_%M_%S", time.localtime())
    filename = timestamp + "_angles_log.csv"
    with open(filename, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(["timestamp", "pan_angle", "tilt_angle"])
        pan_angle, tilt_angle = get_angles()
        while abs(abs(t_pan_angle)-abs(pan_angle)) > delta or abs(abs(t_tilt_angle) - abs(tilt_angle)) > delta:
            # Read angles from the device.
            angles = get_angles()            
            if angles is None:
                pan_angle, tilt_angle = (None, None)
            else:
                pan_angle, tilt_angle = angles
            timestamp = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())
            writer.writerow([timestamp, pan_angle, tilt_angle])
            time.sleep(1/hz)
    print(f"Logged angles to {filename}")


#######################################
#   Manual Pan/Tilt with Arrow Keys   #
#######################################

def send_pelco_d_command(cmd_bits, data1=0, data2=0):
    global ser, address
    if ser is None or not ser.is_open:
        return
    chksum = (address + 0x00 + cmd_bits + data1 + data2) & 0xFF
    frame = bytearray([0xFF, address, 0x00, cmd_bits, data1, data2, chksum])
    ser.write(frame)
    time.sleep(0.05)
    # print(f"Sent: {frame.hex().upper()}")

def get_current_bits_and_speeds():
    bits = 0x00
    pan_spd = 0
    tilt_spd = 0
    if keyboard.Key.left in pressed_keys:
        bits |= BIT_LEFT
        pan_spd = PAN_SPEED
    elif keyboard.Key.right in pressed_keys:
        bits |= BIT_RIGHT
        pan_spd = PAN_SPEED
    if keyboard.Key.up in pressed_keys:
        bits |= BIT_UP
        tilt_spd = TILT_SPEED
    elif keyboard.Key.down in pressed_keys:
        bits |= BIT_DOWN
        tilt_spd = TILT_SPEED
    return (bits, pan_spd, tilt_spd)

#######################################
#   Software-Based Preset Functions   #
#######################################

def prompt_preset_number(action_name):
    try:
        num_str = prompt(f"{action_name} preset #: ")
        preset_num = int(num_str)
        if 0 <= preset_num <= 255:
            return preset_num
        else:
            print("Invalid preset # (must be 0..255).")
    except Exception as e:
        print("Invalid input. Please repeat.")
    return None

def software_set_preset():
    preset_num = prompt_preset_number("Set")
    if preset_num is None:
        return
    pan_pos = query_pan_position()
    tilt_pos = query_tilt_position()
    if pan_pos[0] is None or tilt_pos[0] is None:
        print("Error: Could not read current pan/tilt.")
        return
    presets[preset_num] = (pan_pos[0], pan_pos[1], tilt_pos[0], tilt_pos[1])
    print(f"Software preset #{preset_num} stored: Pan=({pan_pos[0]:02X}, {pan_pos[1]:02X}), "
          f"Tilt=({tilt_pos[0]:02X}, {tilt_pos[1]:02X})")
    pan_angle, tilt_angle = calculate_angles(pan_pos, tilt_pos)
    print(f"Pan angle: {pan_angle}° Tilt angle: {tilt_angle}°")

def software_call_preset():
    preset_num = prompt_preset_number("Call")
    if preset_num is None:
        return
    if preset_num not in presets:
        print(f"No software preset #{preset_num} stored.")
        return
    pan_msb, pan_lsb, tilt_msb, tilt_lsb = presets[preset_num]
    print(f"Moving to software preset #{preset_num} => Pan=({pan_msb:02X},{pan_lsb:02X}), "
          f"Tilt=({tilt_msb:02X},{tilt_lsb:02X})")
    set_positioning_speed()
    set_pan_position(pan_msb, pan_lsb)
    set_tilt_position(tilt_msb, tilt_lsb)
    pan_angle, tilt_angle = calculate_angles([pan_msb,pan_lsb],[tilt_msb, tilt_lsb])
    print(f"Pan angle: {pan_angle}° Tilt angle: {tilt_angle}°")
    log_angles_to_csv(pan_angle, tilt_angle)

def software_clear_preset():
    preset_num = prompt_preset_number("Clear")
    if preset_num is None:
        return
    if preset_num in presets:
        del presets[preset_num]
        print(f"Cleared software preset #{preset_num}.")
    else:
        print(f"No software preset #{preset_num} found.")

#######################################
#   Speed Prompt for Arrow Keys       #
#######################################

def prompt_speeds():
    global PAN_SPEED, TILT_SPEED, ignore_key_events
    try:
        p_in = int(prompt("Enter new pan speed [0-63]: "))
        t_in = int(prompt("Enter new tilt speed [0-63]: "))
        if 0 <= p_in <= 63:
            PAN_SPEED = p_in
        else:
            print("Pan speed out of range, ignoring.")
        if 0 <= t_in <= 63:
            TILT_SPEED = t_in
        else:
            print("Tilt speed out of range, ignoring.")
        print(f"New arrow-key speeds: pan={PAN_SPEED}, tilt={TILT_SPEED}")
    except Exception as e:
        print("Invalid input; keeping old speeds.")
    bits, p_spd, t_spd = get_current_bits_and_speeds()
    if bits != 0:
        send_pelco_d_command(bits, p_spd, t_spd)

#######################################
#         pynput Callbacks            #
#######################################

def on_press(key):
    global running
    if key == keyboard.Key.esc:
        stop_motion()
    if getattr(key, 'char', None) == 'q':
        stop_motion()
        running = False
        return False
    if key in [keyboard.Key.up, keyboard.Key.down, keyboard.Key.left, keyboard.Key.right]:
        if key not in pressed_keys:
            pressed_keys.add(key)
            bits, p_spd, t_spd = get_current_bits_and_speeds()
            send_pelco_d_command(bits, p_spd, t_spd)
    if hasattr(key, 'char'):
        if key.char == 's':
            software_set_preset()
        elif key.char == 'c':
            software_call_preset()
        elif key.char == 'x':
            software_clear_preset()
        elif key.char == 'v':
            prompt_speeds()
        elif key.char == 't':
            self_check()
        elif key.char == 'a':
            get_angles()

def on_release(key):
    global pressed_keys
    if key not in pressed_keys:
        return
    old_bits, old_p_spd, old_t_spd = get_current_bits_and_speeds()
    pressed_keys.remove(key)
    new_bits, new_p_spd, new_t_spd = get_current_bits_and_speeds()
    changed_bits = old_bits & ~new_bits
    transition_bits = old_bits
    transition_p_spd = old_p_spd
    transition_t_spd = old_t_spd
    if (changed_bits & BIT_UP) or (changed_bits & BIT_DOWN):
        transition_t_spd = 0
    if (changed_bits & BIT_LEFT) or (changed_bits & BIT_RIGHT):
        transition_p_spd = 0
    if changed_bits != 0:
        send_pelco_d_command(transition_bits, transition_p_spd, transition_t_spd)
    if new_bits == 0:
        stop_motion()
    else:
        send_pelco_d_command(new_bits, new_p_spd, new_t_spd)

#######################################
#               MAIN                  #
#######################################

def main():
    print("""
PTU Keyboard Control
====================

Controls:
  Arrow keys = Pan/Tilt
  s          = Set a software-based preset (stores current pan/tilt)
  c          = Call that software-based preset (move PTU to stored pan/tilt)
  x          = Clear a software-based preset
  v          = Adjust speeds
  t          = Self-check for calibration
  a          = Get current pan and tilt angle
  ESC        = Stop motion
  q          = Quit
""")
    global ser, PAN_SPEED, TILT_SPEED, running, listener
    ser = serial.Serial(serial_port, baudrate=2400, timeout=0.5)
    if not ser.is_open:
        ser.open()
    
        print(f"Using default speeds: pan={hex(PAN_SPEED)[2:]} and tilt={hex(TILT_SPEED)[2:]}.")
    
    from pynput import keyboard as kb
    listener = kb.Listener(on_press=on_press, on_release=on_release)
    listener.start()
    try:
        while running:
            time.sleep(0.05)
    except KeyboardInterrupt:
        pass
    finally:
        stop_motion()
        print("Stopped")
        if ser.is_open:
            ser.close()
        listener.stop()
        termios.tcsetattr(fd, termios.TCSADRAIN, original_term)
        print("Exiting.")

if __name__ == "__main__":
    try:
        main()
    finally:
        # Restore the original terminal settings.
        termios.tcsetattr(fd, termios.TCSADRAIN, original_term)
