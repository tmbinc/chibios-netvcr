import socket, struct, serial, sys

print "connecting to NETV2 shell..."

netv = serial.Serial(sys.argv[1], timeout = 1)
netv.write("          \rfpga xvc\r")
while True:
	r = netv.readline()
	print r.strip()
	if r == "XVC MODE ENTERED\r\n":
		break

print "waiting for xvc connection..."

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind(('0.0.0.0', 2542))
s.listen(1)

conn, addr = s.accept()

def sread(n):
	res = ""
	while len(res) != n:
		res += conn.recv(n - len(res))
	return res



while 1:
	command = ""
	while not command.endswith(":"):
		command += sread(1)

	if command == "getinfo:":
		conn.send("xvcServer_v1.0:2048\n")
		continue
	elif command == "settck:":
		conn.send(sread(4))
		continue

	assert command == "shift:"
	length = struct.unpack("<I", sread(4))[0]
	nbytes = (length + 7) / 8
	tmsbits = sread(nbytes)
	tdibits = sread(nbytes)

	bits_to_shift = "".join("".join(i) for i in zip(tmsbits, tdibits))
	netv.write("shift:")
	netv.write(struct.pack("<I", length))

	tdobits = ""
	BS = 16
	for o in range(0, nbytes, BS):
		a = bits_to_shift[o*2:(o+BS)*2]
		print ">>", a.encode('hex')
		netv.write(a)
		nr = min(nbytes - o, BS)
		print "<<", nr
		tdobits += netv.read(nr)
		print "<<", tdobits.encode('hex')

	conn.send(tdobits)

	print "done"
