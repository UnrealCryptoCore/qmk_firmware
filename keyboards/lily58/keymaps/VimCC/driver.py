import sys
import hid

vendor_id = 0x04d8
product_id = 0xeb2d

usage_page = 0xFF60
usage = 0x61
report_length = 32

MSG_OS_HELLO = 1
MSG_OS_BYE = 2
MSG_OS_COPY_START = 3
MSG_OS_COPY_PART = 4
MSG_KB_COPY = 5


def get_raw_hid_interface():
    device_interfaces = hid.enumerate(vendor_id, product_id)
    raw_hid_interfaces = [
        i for i in device_interfaces if i['usage_page'] == usage_page and i['usage'] == usage]

    if len(raw_hid_interfaces) == 0:
        return None

    interface = hid.Device(path=raw_hid_interfaces[0]['path'])

    print(f"Manufacturer: {interface.manufacturer}")
    print(f"Product: {interface.product}")

    return interface


def send_raw_report(data):
    interface = get_raw_hid_interface()

    if interface is None:
        print("No device found")
        sys.exit(1)

    request_data = [0x00] * (report_length + 1)  # First byte is Report ID
    request_data[1:len(data) + 1] = data
    request_report = bytes(request_data)

    print("Request:")
    print(request_report)

    try:
        interface.write(request_report)

        response_report = interface.read(report_length, timeout=1000)

        print("Response:")
        print(response_report)

    finally:
        interface.close()


if __name__ == '__main__':
    send_raw_report([
        MSG_OS_HELLO,
        ord('h'),
        ord('e'),
        ord('l'),
        ord('l'),
        ord('o'),
        0,
    ])
    send_raw_report([
        MSG_OS_COPY_START,
        ord('h'),
        ord('e'),
        ord('l'),
        ord('l'),
        ord('o'),
        0,
    ])
