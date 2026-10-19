"""pymavlink ile altın (golden) test vektörleri üretir.

Bağımsız bir MAVLink gerçeklemesinin (pymavlink) ürettiği baytlar
tests/core/golden_vectors.inc dosyasına yazılır; C++ kodlayıcı aynı alan
değerleriyle bayt bayt aynı çıktıyı, ayrıştırıcı da aynı alanları vermelidir.

    pip install pymavlink==2.4.49
    python tools/gen_golden.py > tests/core/golden_vectors.inc

Alan değerleri tests/core/test_golden.cpp içindeki değerlerle eşleşmelidir.
"""
import sys

from pymavlink.dialects.v10 import common as v1
from pymavlink.dialects.v20 import common as v2


def packed(dialect, msg, seq, sysid=1, compid=1):
    mav = dialect.MAVLink(None, srcSystem=sysid, srcComponent=compid)
    mav.seq = seq
    return bytes(msg.pack(mav))


def vec(name, data):
    body = ", ".join(f"0x{b:02X}" for b in data)
    return f"static const std::vector<std::uint8_t> k{name} = {{{body}}};\n"


out = []
out.append(vec("HeartbeatV2", packed(v2, v2.MAVLink_heartbeat_message(
    type=2, autopilot=0, base_mode=129, custom_mode=1, system_status=4, mavlink_version=3), seq=7)))
out.append(vec("HeartbeatV1", packed(v1, v1.MAVLink_heartbeat_message(
    type=2, autopilot=0, base_mode=129, custom_mode=1, system_status=4, mavlink_version=3), seq=7)))
out.append(vec("GlobalPositionIntV2", packed(v2, v2.MAVLink_global_position_int_message(
    time_boot_ms=123456, lat=398890000, lon=327785000, alt=1000500, relative_alt=50500,
    vx=-120, vy=340, vz=-25, hdg=27050), seq=200)))
out.append(vec("AttitudeV2", packed(v2, v2.MAVLink_attitude_message(
    time_boot_ms=1000, roll=0.5, pitch=-0.25, yaw=1.5, rollspeed=0.0, pitchspeed=0.0,
    yawspeed=0.0), seq=1)))
out.append(vec("CommandLongV2", packed(v2, v2.MAVLink_command_long_message(
    target_system=1, target_component=154, command=1000, confirmation=0,
    param1=-45.0, param2=30.0, param3=0.0, param4=0.0, param5=0.0, param6=0.0, param7=0.0),
    seq=3, sysid=255, compid=190)))
out.append(vec("MissionItemIntV2", packed(v2, v2.MAVLink_mission_item_int_message(
    target_system=1, target_component=1, seq=2, frame=3, command=16, current=0,
    autocontinue=1, param1=5.0, param2=2.0, param3=0.0, param4=0.0,
    x=398895000, y=327790000, z=60.0, mission_type=0), seq=9, sysid=255, compid=190)))
out.append(vec("CommandAckV2", packed(v2, v2.MAVLink_command_ack_message(
    command=400, result=0, progress=0, result_param2=0, target_system=255,
    target_component=190), seq=11)))
out.append(vec("StatusTextV2", packed(v2, v2.MAVLink_statustext_message(
    severity=6, text=b"Gorev basladi", id=0, chunk_seq=0), seq=12)))
out.append(vec("SysStatusV2", packed(v2, v2.MAVLink_sys_status_message(
    onboard_control_sensors_present=0, onboard_control_sensors_enabled=0,
    onboard_control_sensors_health=0, load=310, voltage_battery=16520, current_battery=1450,
    battery_remaining=82, drop_rate_comm=0, errors_comm=0, errors_count1=0, errors_count2=0,
    errors_count3=0, errors_count4=0), seq=13)))

sys.stdout.write("// tools/gen_golden.py tarafından pymavlink "
                 "2.4.49 ile üretildi; elle düzenlemeyin.\n")
sys.stdout.write("".join(out))
