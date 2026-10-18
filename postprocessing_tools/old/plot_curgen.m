% Settings
filename = 'data/samples.txt';
dim = 2;                    % Number of parameter columns
color_by_likelihood = false;

% Locate plotmatrix_hist.m beside this script
addpath(fileparts(mfilename('fullpath')));

% Load samples
data = load(filename);
assert(size(data, 2) >= dim, 'Insufficient parameter columns.');
theta = data(:, 1:dim);

% Plot
figure;
if color_by_likelihood
    assert(size(data, 2) >= dim + 1, 'Missing log-likelihood column.');
    plotmatrix_hist(theta, data(:, dim + 1));
else
    plotmatrix_hist(theta);
end
