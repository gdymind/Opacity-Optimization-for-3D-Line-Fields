flag = true;

O = ones(1, segmentNum);

c = -1 * ones(segmentNum, 1);
lb = zeros(segmentNum, 1);
ub = ones(segmentNum, 1);

options = optimoptions('quadprog','Algorithm','trust-region-reflective');
