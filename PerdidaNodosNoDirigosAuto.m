clc;
clear;

%% DEFINICIÓN DEL GRAFO
% Matriz de adyacencia (no dirigida)
A = [
     0 1 1 0 0 0 0 0;
     1 0 1 0 0 1 0 0;
     1 1 0 1 0 1 1 1;
     0 0 1 0 1 0 0 1;
     0 0 0 1 0 0 0 1;
     0 1 1 0 0 0 1 0;
     0 0 1 0 0 1 0 1;
     0 0 1 1 1 0 1 0
];

% A = [
%     0 1 1 0 0 0 0 0;  % 1 -> {2,3}
%     1 0 1 0 0 1 0 0;  % 2 -> {1,3,6}
%     1 1 0 1 0 0 1 0;  % 3 -> {1,2,4,7}
%     0 0 1 0 1 0 0 0;  % 4 -> {3,5}
%     0 0 0 1 0 0 0 1;  % 5 -> {4,8}
%     0 1 0 0 0 0 1 0;  % 6 -> {2,7}
%     0 0 1 0 0 1 0 1;  % 7 -> {3,6,8}
%     0 0 0 0 1 0 1 0   % 8 -> {5,7}
% ];


n = size(A, 1);  % número de nodos

% vecinos de cada nodo basado en matriz de adyacencia (verificación)
fprintf('Lista de vecinos por nodo:\n');
for i = 1:n
    vecinos = find(A(i,:) == 1);
    fprintf('Nodo %d: vecinos → ', i);
    fprintf('%d ', vecinos);
    fprintf('\n');
end

%% VISUALIZACIÓN DEL GRAFO ORIGINAL
G = graph(A); % Crea el grafo original
fig = figure;
p = plot(G, 'Layout', 'force', 'LineWidth', 2);
title('Grafo original sin eliminar nodos');

% Guardamos las posiciones para usarlas más adelante
pos_original = [p.XData(:), p.YData(:)];

resp = input('\n¿Deseas eliminar nodos? (s/n): ', 's');

if lower(resp) == 's'
    %% INGRESO DE NODOS A ELIMINAR
    fprintf('\nEste grafo tiene %d nodos (1 a %d).\n', n, n);
    n_quitar = input('¿Cuántos nodos deseas eliminar? ');
    nodos_quitar = input('Indica los nodos a eliminar como vector (ej. [1 2]): ');

    %% CÁLCULO AUTOMÁTICO  
    syms lambda

    % Paso 1: Eliminar nodos de la matriz de adyacencia
    A_red = A;
    A_red(nodos_quitar, :) = [];
    A_red(:, nodos_quitar) = [];

    % Paso 2: Calcular matriz de grado
    D_red = diag(sum(A_red, 2));

    % Paso 3: Calcular matriz Laplaciana
    L_red = D_red - A_red;

    % Paso 4: Matriz característica y polinomio
    I = sym(eye(size(L_red)));
    M = L_red - lambda * I;
    char_poly = simplify(det(M));
    eigenvals = solve(char_poly == 0, lambda);

    %% RESULTADOS
    disp('Matriz Laplaciana reducida:')
    disp(L_red)

    disp('Polinomio característico:')
    pretty(char_poly)

    disp('Autovalores simbólicos:')
    disp(eigenvals)

    disp('Autovalores numéricos aproximados:')
    disp(double(eigenvals))

    disp('Listo :D')

    %% VISUALIZACIÓN DEL GRAFO REDUCIDO
    nodos_restantes = setdiff(1:n, nodos_quitar);         
    etiquetas = arrayfun(@num2str, nodos_restantes, 'UniformOutput', false); 
    
    G_red = graph(A_red, etiquetas);    
    
    % Extraer las posiciones solo de los nodos restantes
    pos_reducido = pos_original(nodos_restantes, :); 
    
    % Graficar usando posiciones originales
    figure;
    plot(G_red, 'XData', pos_reducido(:,1), 'YData', pos_reducido(:,2), ...
         'LineWidth', 2, 'NodeLabel', etiquetas);
    title('Grafo después de eliminar nodos');

else
    %% VISUALIZACIÓN DEL GRAFO ORIGINAL
    disp('FIN')
end