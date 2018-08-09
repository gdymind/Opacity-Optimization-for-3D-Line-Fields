flag = true;

O = ones(1, segmentNum);
D = zeros(segmentNum, segmentNum);
I = eye(segmentNum);

for i = 1 : segmentNum
	if mod(i-1, segPerLine) ~= 0
		D(i,i) = 1;
		D(i,i-1) = -1;
	end
end

G = diag(G_diag);
