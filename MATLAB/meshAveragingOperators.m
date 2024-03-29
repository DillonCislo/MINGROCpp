function [V2F, F2V] = meshAveragingOperators(F, V, weightType)
%MESHAVERAGINOPERATORS Construct sparse operators that can convert
%quantities from faces to vertices and vice versa
%
%   INPUT PARAMETERS:
%
%       - F             #Fx3 face connectivty list
%       - V             #VxD vertex coordinate list
%       - weightType    The type of weighting used to average face-based
%                       quantities onto vertices:
%                       - 'uniform'
%                       - 'area'
%                       - 'angle'
%
%   OUTPUT PARAMETERS:
%
%       - V2F       #Fx#V sparse matrix that averages face based quantities
%                   onto vertices
%
%       - F2V       #Vx#F sparse matrix that averages vertex based
%                   quantities onto faces using normalized angle weights
%
%
%   by Dillon Cislo 06/23/2020

% Input processing --------------------------------------------------------
if (nargin < 3), weightType = 'angle'; end

validateattributes(V, {'numeric'}, {'2d', 'finite', 'real'});
validateattributes(F, {'numeric'}, ...
    {'2d', 'ncols', 3, 'real', 'integer', 'positive', '<=', size(V,1)});

% A MATLAB-style representation of the triangulation
TR = triangulation(F, V);

% The edge connectivty list
E = TR.edges;

validateattributes( weightType, {'char'}, {'vector'} )
weightType = lower(weightType);
if ~ismember( weightType, {'uniform', 'area', 'angle'} )
    error('Invalid weighting type supplied!');
end

% Construct face-edge correspondence tool ---------------------------------
% Given a list of scalar edge quantities, 'EQ', the output of
% 'EQ(feIDx(f,i))' is that quantity corresponding to the edge
% opposite the ith vertex in face f

e1IDx = sort( [ F(:,3), F(:,2) ], 2 );
e2IDx = sort( [ F(:,1), F(:,3) ], 2 );
e3IDx = sort( [ F(:,2), F(:,1) ], 2 );

[~, e1IDx] = ismember( e1IDx, E, 'rows' );
[~, e2IDx] = ismember( e2IDx, E, 'rows' );
[~, e3IDx] = ismember( e3IDx, E, 'rows' );

feIDx = [ e1IDx e2IDx e3IDx ];

% Construct the vertex-to-face operator -----------------------------------
V2F = sparse(repmat(1:size(F,1), 3, 1), F.', 1/3, ...
    size(F,1), size(V,1));

% Construct the face-to-vertex operator -----------------------------------

switch weightType
    
    case 'uniform'
        
        % Calculate the number of faces attached to each vertex
        numFaceV = full(sparse( F(:), 1, 1, size(V,1), 1));
        
        % Invert to find the per-vertex weight
        numFaceV = 1 ./ numFaceV;
        
        % The averaging weights
        W = numFaceV(F(:));
        
    case 'area'
        
        % The edge lengths of the triangulation
        L = V(E(:,2), :) - V(E(:,1), :);
        L = sqrt(sum(L.^2, 2));
        
        % Calculate the area of each face
        L_F = L(feIDx);
        S_F = sum(L_F, 2) ./ 2;
        faceAreas = sqrt( S_F .* (S_F - L_F(:,1)) .* ...
            (S_F - L_F(:,2)) .* (S_F - L_F(:,3)) );
        faceAreas = repmat(faceAreas, 3, 1);
        
        % The sum of all the face areas around each vertex
        areaSumV = full(sparse( F(:), 1, faceAreas, size(V,1), 1));
        
        % The averaging weights
        W = faceAreas ./ areaSumV(F(:));
        
    case 'angle'
        
        % The edge lengths of the triangulation
        L = V(E(:,2), :) - V(E(:,1), :);
        L = sqrt(sum(L.^2, 2));
        
        % Calculate internal angles
        Gi = L(feIDx);
        Gj = circshift(Gi, [0 -1]);
        Gk = circshift(Gi, [0 -2]);
        intAngles = ( Gj.^2 + Gk.^2 - Gi.^2 ) ./ ( 2 .* Gj .* Gk );
        intAngles = acos(intAngles);
        intAngles = intAngles(:);
        
        % The sum of the internal around each vertex
        angSumV = full(sparse( F(:), 1, intAngles, size(V,1), 1 ));
        
        % The averaging weights
        W = intAngles ./ angSumV(F(:));
        
end

F2V = sparse(F(:), repmat(1:size(F,1),1,3), W, ...
    size(V,1), size(F,1));

end

