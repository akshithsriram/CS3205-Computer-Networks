import math
import os
import sys
import argparse
import random
import matplotlib.pyplot as plt

def ReadArgments():
	parser = argparse.ArgumentParser()

	# ./mytcp -i <double> -m <double> -n <double> -f <double> -s <double> -T <int> -o outfile

	# optional arguments
	parser.add_argument("-i", "--init", type=float, nargs='?', default=1, const=1)
	parser.add_argument("-m", "--mult_exp", type=float, nargs='?', default=1, const=1)
	parser.add_argument("-n", "--mult_lin", type=float, nargs='?', default=1, const=1)

	# mandatory arguments
	parser.add_argument("-f", "--mult_tim", type=float, help="Multiplier for Timeout")
	parser.add_argument("-s", "--prob", type=float, help="Prob of ACK before timeout")
	parser.add_argument("-T", "--total", type=int, help="Total no. of segments")
	parser.add_argument("-o", "--outfile", type=str, help="Output file")

	args = parser.parse_args()

	try:	
		Ki = float(args.init)
	except:
		sys.exit("Enter valid -i argument")
	try:	
		Km = float(args.mult_exp)
	except:
		sys.exit("Enter valid -m argument")
	try:	
		Kn = float(args.mult_lin)
	except:
		sys.exit("Enter valid -n argument")
	try:	
		Kf = float(args.mult_tim)
	except:
		sys.exit("Enter valid -f argument")
	try:	
		Ps = float(args.prob)
	except:
		sys.exit("Enter valid -s argument")
	try:	
		T = int(args.total)
	except:
		sys.exit("Enter valid -T argument")
	outfile = str(args.outfile)
	if outfile == None:
		sys.exit("Enter valid -o argument")	

	return Ki, Km, Kn, Kf, Ps, T, outfile

def CheckArguments(Ki, Km, Kn, Kf, Ps, T):
	if Ki < 1 or Ki > 4:
		sys.exit("Invalid value of Ki (Initial congestion window)")
	if Km < 0.5 or Km > 2:
		sys.exit("Invalid value of Km (multiplier for exponential)")
	if Kn < 0.5 or Kn > 2:
		sys.exit("Invalid value of Kn (multiplier for linear)")	
	if Kf < 0.1 or Kf > 0.5:
		sys.exit("Invalid value of Kf (multiplier for timeout)")	
	if Ps < 0 or Ps > 1:
		sys.exit("Invalid value of Ps (probabilitiy of ACK before timeout)")	

def ComputeReceiveSize(send_size, Ps):
	count = 0
	for i in range(send_size):
		estimate = random.random()
		if estimate <= Ps:
			count += 1
	return count

def Simulate_TCPCongestionControl(Ki, Km, Kn, Kf, Ps, T):
	RWS = 1024 	# 1 MB
	MSS = 1 	# 1 KB
	threshold = RWS/2
	segments_sent = 0
	
	current_CW = Ki * MSS
	linear = 0
	rounds = 1
	
	Rounds = []
	Transmission_Rounds = []
	Transmission_values = []
	CW_values = []
	batch = 0
	window_size = current_CW

	Transmission_Rounds.append(rounds)
	Transmission_values.append(current_CW)
	rounds += 1

	while segments_sent < T:
		current_send_size = math.ceil(current_CW)
		estimate = random.random()
		if estimate > Ps:
			# Timeout condition
			threshold = current_CW/2
			current_CW = max(1, Kf * current_CW)
			linear = 0  # Change to exponential
		else:
			if linear == 0:
				# exponential growth phase
				current_CW = min(current_CW + Km * MSS, RWS)
				if current_CW >= threshold:
					current_CW = threshold
					linear = 1
			else:
				# linear growth phase
				current_CW = min(current_CW + Kn*(MSS*MSS)/current_CW, RWS)
		
		segments_sent += 1
		batch += 1
		
		Rounds.append(segments_sent)
		if batch == window_size:
			Transmission_Rounds.append(rounds)
			Transmission_values.append(current_CW)
			rounds += 1
			batch = 0
			window_size = math.ceil(current_CW)
		CW_values.append(current_CW)

	if batch != 0:
		Transmission_Rounds.append(rounds)
		Transmission_values.append(current_CW)
			
	return CW_values, Rounds, Transmission_Rounds, Transmission_values

def Plot_Rounds(Transmission_Rounds, Transmission_values, title, outfile):
	plt.plot(Transmission_Rounds, Transmission_values, marker='o')
	plt.title(title)
	plt.xlabel("Transmission Rounds")
	plt.ylabel("Congestion Window Size")

	if not os.path.exists('Plots'):
		os.makedirs('Plots')
	plt.savefig("Plots/" + outfile + "-2.jpg")
	#plt.show()
	plt.close()

def Plot_UpdateNumber(rounds, CW_values, title, outfile):
	plt.plot(rounds, CW_values, marker='o')
	plt.title(title)
	plt.xlabel("Update Number")
	plt.ylabel("Congestion Window Size")

	if not os.path.exists('Plots'):
		os.makedirs('Plots')
	plt.savefig("Plots/" + outfile + "-1.jpg")
	#plt.show()
	plt.close()

def main():

	# Read and Check input arguments
	Ki, Km, Kn, Kf, Ps, T, outfile = ReadArgments()
	CheckArguments(Ki, Km, Kn, Kf, Ps, T)

	CW_values, rounds, Transmission_Rounds, Transmission_values = Simulate_TCPCongestionControl(Ki, Km, Kn, Kf, Ps, T)
	

	title = "Parameters Ki = " + str(Ki) + ", Km = " + str(Km) + ", Kn = " + str(Kn) + ", Kf = " + str(Kf) + ", Ps = " + str(Ps) + ", T = " + str(T)
	Plot_UpdateNumber(rounds, CW_values, title, outfile)
	Plot_Rounds(Transmission_Rounds, Transmission_values, title, outfile)


	if not os.path.exists('OutputFiles'):
		os.makedirs('OutputFiles')
	with open("OutputFiles/" + outfile + ".txt", 'w') as f:
		for cw in CW_values:
			f.write(str(cw) + "\n")

if __name__ == "__main__":
	main()