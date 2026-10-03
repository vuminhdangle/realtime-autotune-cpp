import numpy as np
from matplotlib import pyplot as plt
from scipy.io.wavfile import write

# --> Adapt the file name (and path) <--
filename = 'dump_out'

# ---- Read binary file (double format) -------
# --> change format if necessary <--
x = np.fromfile(filename, dtype=np.double)

# ---- Write wav file -------------------------
# Write signal as a .wav file (useless if signal is not audio). 
# The sampling rate is hard coded here, --> change it if necessary <--
write(filename + '.wav', 44100, x)

# ---- Plot -----------------------------------
# --> Adapt the text on the Figure <--
plt.plot(x)
plt.xlabel('[A compléter]')
plt.ylabel('[A compléter]')
plt.title('[A compléter]')
plt.grid()
plt.show()

