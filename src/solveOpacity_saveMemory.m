H = H.';

%Q = (1) + (4)
Q = zeros(segmentNum, segmentNum);
for i = 1 : segmentNum
	if mod(i-1, segPerLine) ~= 0
		Q(i,i) = 1;
		Q(i,i-1) = -1;
	end
end
Q = coff(4) * (Q.' * Q);
Q = Q + eye(segmentNum);

G = diag(G_diag);
W = ((eye(segmentNum)-G).^coff(5));
W = W * H;
clear H;
W = W * G;
clear G;

Q = Q + coff(2) * (W * W.');
Q = Q  + coff(3) * (W.' * W);
clear W;

if (size(O, 1) == 1)
    O = O.';
end
O = quadprog(Q, c, [], [], [], [], lb, ub, O, options);
O = O.';

flag = true;