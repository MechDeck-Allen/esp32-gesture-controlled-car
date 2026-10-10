# ESP-NOW communication

ESP-NOW is a connectionless protocol carried by the ESP32 Wi-Fi radio. It allows the two ESP32s to exchange short packets without a Wi-Fi router or internet.

The transmitter broadcasts a packed packet containing:
- packet type (`TYPE_CONTROL`)
- sequence number
- relative pitch and roll angles
- calibration status

The receiver parses control packets and broadcasts an acknowledgement (`TYPE_ACK`) with the sequence number. Both boards must use the same Wi-Fi channel (channel 1 in these sketches) and matching packet structures.

The receiver uses a 350 ms packet timeout as a motor failsafe. The transmitter LED indicates recent ACK reception; the receiver LED indicates recent control packet reception.

This prototype uses broadcast packets without encryption. For a later hardened version, use known peer MAC addresses, encryption where appropriate, packet validation and sequence freshness checks.
