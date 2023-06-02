clc;
clear;
N = 512;
k = 0:(N/2)-1;
w_r = cos(2*pi*k/N);
w_i = sin(-2*pi*k/N);
writematrix(w_r,'weights_real.txt','Delimiter',',');
writematrix(w_i,'weights_imag.txt','Delimiter',',');