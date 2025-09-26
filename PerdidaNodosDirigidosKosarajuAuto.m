clc; clear;

%% ================== 1) MATRIZ DE INCIDENCIA DIRIGIDA ====================
% Convención: -1 en ORIGEN, +1 en DESTINO, 0 en el resto (filas=nodos, cols=arcos).
% Ejemplo: ajusta B a tu caso real

B = [ ...
   -1, +1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1,  0,  0,  0, +1,  0;  % 1
   +1,  0, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, +1, +1,  0,  0, +1;  % 2
    0, -1, +1,  0, +1, -1,  0, +1, -1,  0,  0,  0,  0, -1,  0,  0,  0,  0,  0,  0,  0;  % 3
    0,  0,  0,  0,  0,  0,  0,  0, +1, -1, +1,  0,  0,  0, -1,  0,  0,  0,  0,  0,  0;  % 4
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1, +1,  0, +1,  0,  0, -1,  0,  0, -1,  0;  % 5
    0,  0,  0, +1, -1,  0,  0,  0,  0,  0,  0,  0, -1,  0,  0, +1,  0,  0, +1,  0,  0;  % 6
    0,  0,  0,  0,  0, +1, -1,  0,  0,  0,  0,  0, +1,  0, +1,  0,  0,  0,  0,  0, -1;  % 7
    0,  0,  0,  0,  0,  0, +1, -1,  0, +1,  0, -1,  0,  0,  0,  0,  0, -1, -1,  0,  0]; % 8

n = size(B,1);

% Layout fijo como en los otros grafos para comparacion
useFixedLayout = true;
X = [ 0.00, -0.85,  0.10,  0.95,  1.25, -1.05, -0.60,  0.35 ];
Y = [-1.05, -0.75, -0.30,  0.10,  0.70, -0.15,  0.45,  0.40 ];

%% ============= 2) Incidencia a aristas (s,t) y grafo dirigido ===========
[s_all, t_all] = incidence_to_edges(B);              % 1..n
labels = arrayfun(@num2str, 1:n, 'UniformOutput', false);
G = digraph(s_all, t_all, [], labels);

figure(1); clf
if useFixedLayout && numel(X)==n && numel(Y)==n
    plot(G, 'XData', X, 'YData', Y);
else
    plot(G);
end
title('Grafo original (incidencia)');

%% ============= 3) Pedir nodos a eliminar ================================
fprintf('\nEste grafo tiene %d nodos (etiquetas 1..%d).\n', n, n);
txt = input('Indica los nodos a eliminar como vector (ej. [2 5]) o [] para ninguno: ');
if isempty(txt)
    nodos_quitar = [];
else
    nodos_quitar = unique(txt(:)');   % limpio
    if any(nodos_quitar < 1 | nodos_quitar > n)
        error('Algún índice de nodo a eliminar está fuera de 1..%d', n);
    end
end

keep = setdiff(1:n, nodos_quitar);
if isempty(keep)
    error('Eliminaste todos los nodos; no hay grafo que evaluar.');
end

% Filtrar aristas que se quedan completamente dentro de "keep"
mask_keep_edges = ismember(s_all, keep) & ismember(t_all, keep);
s_red = s_all(mask_keep_edges);
t_red = t_all(mask_keep_edges);

% Remapear a índices compactos para dibujar
map = zeros(1,n); map(keep) = 1:numel(keep);
s_plot = map(s_red);
t_plot = map(t_red);
etiquetas = arrayfun(@num2str, keep, 'UniformOutput', false);

% Layout filtrado si lo estás usando
if useFixedLayout && numel(X)>=max(keep) && numel(Y)>=max(keep)
    X_red = X(keep); Y_red = Y(keep);
else
    X_red = []; Y_red = [];
end

% Construir y dibujar el grafo reducido
G_red = digraph(s_plot, t_plot, [], etiquetas);
figure(2); clf
if ~isempty(X_red)
    plot(G_red, 'XData', X_red, 'YData', Y_red);
else
    plot(G_red);
end
title(sprintf('Grafo tras eliminar nodos %s', mat2str(nodos_quitar)));

%% ============= 4) Verificación de FUERTE CONECTIVIDAD ====================
nr = numnodes(G_red);
if nr == 1
    % Si quieres EXCLUIR singleton, pon: isSC = false;
    isSC = true; 
elseif nr > 1
    comp = conncomp(G_red, 'Type', 'strong');
    isSC = (max(comp) == 1);
else
    isSC = false;
end

fprintf('\nSubgrafo: %d nodos, %d arcos.\n', nr, numedges(G_red));
if isSC
    disp('Resultado: El grafo es FUERTEMENTE CONEXO.');
else
    disp('Resultado: El grafo NO es fuertemente conexo.');
end

% (Opcional) imprime arcos del subgrafo con etiquetas originales
[s_idx, t_idx] = findedge(G_red);
if ~isempty(s_idx)
    fprintf('Arcos (etiquetas originales):\n');
    for k = 1:numel(s_idx)
        fprintf('  %s -> %s\n', G_red.Nodes.Name{s_idx(k)}, G_red.Nodes.Name{t_idx(k)});
    end
else
    fprintf('Sin arcos en el subgrafo.\n');
end

%% ======================= funciones locales =====================
function [s, t] = incidence_to_edges(B)
    % Cada columna debe tener exactamente un -1 (origen) y un +1 (destino)
    [n, m] = size(B);
    s = zeros(1,m); t = zeros(1,m);
    k = 0;
    for e = 1:m
        src = find(B(:,e) == -1);
        dst = find(B(:,e) == +1);
        if numel(src)==1 && numel(dst)==1
            k = k + 1;
            s(k) = src; t(k) = dst;
        end
        % columnas con formato invalido se ignoran
    end
    s = s(1:k); t = t(1:k);
end
