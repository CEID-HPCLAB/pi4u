graphics_toolkit('gnuplot');
setenv('GNUTERM', 'pngcairo');
set(0, 'defaultfigurevisible', 'off');

data = load('data/samples.txt');
dim = 2;

figure;
plotmatrix(data(:, 1:dim));
print('data/samples_octave.png', '-dpng', '-r150');
close;
