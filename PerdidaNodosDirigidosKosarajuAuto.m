clc; clear;

%% MATRIZ DE ADYACENCIA DIRIGIDA (8 nodos)
% A = [
%     0 1 1 0 0 0 0 0;  % 1 -> {2,3}
%     1 0 1 0 0 1 0 0;  % 2 -> {1,3,6}
%     1 1 0 1 0 1 1 1;  % 3 -> {1,2,4,6,7,8}
%     0 0 1 0 1 0 0 1;  % 4 -> {3,5,8}
%     0 0 0 1 0 0 0 1;  % 5 -> {4,8}
%     0 1 1 0 0 0 1 0;  % 6 -> {2,3,7}
%     0 0 1 0 0 1 0 1;  % 7 -> {3,6,8}
%     0 0 1 1 1 0 1 0   % 8 -> {3,4,5,7}
% ];

A = [
    0 1 1 0 0 0 0 0;  % 1 -> {2,3}
    1 0 1 0 0 1 0 0;  % 2 -> {1,3,6}
    1 1 0 1 0 0 1 0;  % 3 -> {1,2,4,7}
    0 0 1 0 1 0 0 0;  % 4 -> {3,5}
    0 0 0 1 0 0 0 1;  % 5 -> {4,8}
    0 1 0 0 0 0 1 0;  % 6 -> {2,7}
    0 0 1 0 0 1 0 1;  % 7 -> {3,6,8}
    0 0 0 0 1 0 1 0   % 8 -> {5,7}
];
n = size(A,1);

% (Solo para verificación)
fprintf('Vecinos por nodo:\n');
for i = 1:n
    v = find(A(i,:)==1);
    fprintf('Nodo %d: ', i); fprintf('%d ', v); fprintf('\n');
end

%% Grafo original
G = digraph(A);

% Posiciones utilizadas en el grafo no dirigido para comparar (se puede
% quitar pero es para una mejor visualización)
X = [ 0.00, -0.85,  0.10,  0.95,  1.25, -1.05, -0.60,  0.35 ];
Y = [-1.05, -0.75, -0.30,  0.10,  0.70, -0.15,  0.45,  0.40 ];

figure; clf
plot(G,'XData',X,'YData',Y);  
title('Grafo original sin eliminar nodos');

%% ¿Eliminar nodos?
resp = input('\n¿Deseas eliminar nodos? (s/n): ','s');
if lower(resp)=='s'
    fprintf('\nEste grafo tiene %d nodos (1 a %d).\n', n, n);
    nodos_quitar = input('Indica los nodos a eliminar como vector (ej. [2 5]): ');

    % Reducir matriz
    A(nodos_quitar,:) = [];
    A(:,nodos_quitar) = [];

    % Nodos restantes (mantener etiquetas originales)
    nodos_restantes = setdiff(1:n, nodos_quitar);
    etiquetas = arrayfun(@num2str, nodos_restantes, 'UniformOutput', false);

    % Posiciones filtradas con el mismo layout
    X_red = X(nodos_restantes);
    Y_red = Y(nodos_restantes);

    % Grafo reducido con mismo estilo/posiciones
    G_red = digraph(A, etiquetas);
    figure; clf
    plot(G_red,'XData',X_red,'YData',Y_red);
    title('Grafo después de eliminar nodos');
    
    % Actualizar n para la verificación
    n = size(A,1);
end

%% Lista de adyacencia (matriz actual A)
adjList = cell(1,n);
for i = 1:n, adjList{i} = find(A(i,:)==1); end

%% Verificación de fuerte conectividad (DFS)
isStronglyConnected = true;
for i = 1:n
    visited = false(1,n);
    stack = i; visited(i)=true;
    while ~isempty(stack)
        node = stack(end); stack(end)=[];
        for neigh = adjList{node}
            if ~visited(neigh)
                stack(end+1)=neigh; visited(neigh)=true;
            end
        end
    end
    if any(~visited), isStronglyConnected=false; break; end
end

%% Resultado
if isStronglyConnected
    disp('El grafo es FUERTEMENTE CONEXO.');
else
    disp('El grafo NO es fuertemente conexo.');
end
