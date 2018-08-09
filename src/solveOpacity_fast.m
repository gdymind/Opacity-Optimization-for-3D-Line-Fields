H = H.';

W = ((I-G)^coff(5)) * H * G;

Q = I + coff(2) * (W * W.') + coff(3) * (W.' * W) + coff(4) * (D.' * D);

c = -1 * ones(segmentNum, 1);

lb = zeros(segmentNum, 1);
ub = ones(segmentNum, 1);

if (size(O, 1) == 1)
    O = O.';
end
O = quadprog(Q, c, [], [], [], [], lb, ub, O);
O = O.';

flag = true;