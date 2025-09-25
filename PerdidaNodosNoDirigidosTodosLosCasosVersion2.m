% =========================================================================
% analiza_lambda2_eliminaciones.m
% Calcula la conectividad algebraica (lambda2) del laplaciano para
% todas las combinaciones de eliminación de nodos de un grafo no dirigido.
% Guarda resultados en Excel y CSV.
% =========================================================================

clc; clear;

%% === 1) MATRIZ DE ADJACENCIA (EJEMPLO DEL USUARIO) ======================
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
n = size(A,1);

%% === 2) PARÁMETROS DEL ANÁLISIS ========================================
% Elimina hasta 'maxRemove' nodos (por defecto n-1 = todas las combinaciones posibles)
maxRemove = n - 1;   % reduce si quieres acotar la explosión combinatoria (p.ej., 3)

% Tolerancia numérica para decidir conectividad (evita negativos diminutos por redondeo)
tol = 1e-9;

% (Opcional) Analizar SOLO un conjunto específico de eliminaciones:
%  - Deja 'use_only_list' = false para barrer TODAS las combinaciones hasta maxRemove
%  - Si lo pones en true, define 'remove_list' con vectores de nodos a eliminar
use_only_list = false;
remove_list = { [1], [2], [2 3], [3 4 5] };  % ejemplo; edita si activas 'use_only_list'

%% === 3) PREPARAR TABLA DE RESULTADOS ====================================
vars  = {'Removed','Remaining','lambda2','lambda2_raw','Connected'};
types = {'string', 'double', 'double', 'double', 'logical'};
results = table('Size',[0 numel(vars)], 'VariableTypes', types, 'VariableNames', vars);

fprintf('\nIniciando análisis de lambda2...\n');

%% === 4) CÁLCULO =========================================================
if ~use_only_list
    % Recorre todas las combinaciones de 0..maxRemove nodos eliminados
    for k = 0:maxRemove
        combos = nchoosek(1:n, k);   % todas las combinaciones de tamaño k
        fprintf('  k = %d => %d casos\n', k, size(combos,1));
        for r = 1:size(combos,1)
            removed = combos(r,:);
            [lam2_raw, lam2, isconn, remaining] = lambda2_after_removal(A, removed, tol);
            results = [results; {
                sprintf('[%s]', strjoin(string(removed),',')), ...
                remaining, ...
                lam2, ...
                lam2_raw, ...
                isconn}];
        end
    end
else
    % Solo los casos listados en 'remove_list'
    fprintf('  Usando lista específica de eliminaciones (%d casos)\n', numel(remove_list));
    for i = 1:numel(remove_list)
        removed = remove_list{i};
        [lam2_raw, lam2, isconn, remaining] = lambda2_after_removal(A, removed, tol);
        results = [results; {
            sprintf('[%s]', strjoin(string(removed),',')), ...
            remaining, ...
            lam2, ...
            lam2_raw, ...
            isconn}];
    end
end

%% === 5) RESÚMENES RÁPIDOS EN CONSOLA ===================================
% Máscaras de utilidad
mask_valid = ~isnan(results.lambda2);           % subgrafos con >= 2 nodos
mask_conn  = mask_valid & results.Connected;    % ...y además conexos (lambda2 > tol)

% Conteos y porcentajes
n_total_valid = sum(mask_valid);
n_conn        = sum(mask_conn);
n_disc        = n_total_valid - n_conn;

if n_total_valid > 0
    pct_conexos = 100 * n_conn / n_total_valid;
    pct_nocon   = 100 * n_disc / n_total_valid;
else
    pct_conexos = NaN; pct_nocon = NaN;
end

fprintf('\n=== Resumen de conectividad (entre subgrafos con >= 2 nodos) ===\n');
fprintf('Evaluados : %d\n', n_total_valid);
fprintf('Conexos   : %d (%.2f%%)\n', n_conn, pct_conexos);
fprintf('No conexos: %d (%.2f%%)\n', n_disc, pct_nocon);

% Peores 5 casos SOLO entre grafos CONEXOS (lambda2 mínima)
if n_conn > 0
    results_conn = results(mask_conn, :);
    [~, idxAscConn] = sort(results_conn.lambda2, 'ascend', 'MissingPlacement','last');
    fprintf('\nPeores 5 casos entre CONEXOS (lambda2 mínima > tol):\n');
    disp(results_conn(idxAscConn(1:min(5, height(results_conn))), :))
else
    fprintf('\nNo hay subgrafos conexos para listar peores casos.\n');
end

% Mejores 5 casos SOLO entre grafos CONEXOS (lambda2 máxima)
if n_conn > 0
    [~, idxDescConn] = sort(results_conn.lambda2, 'descend', 'MissingPlacement','last');
    fprintf('Mejores 5 casos entre CONEXOS (lambda2 máxima):\n');
    disp(results_conn(idxDescConn(1:min(5, height(results_conn))), :))
end

%% === 6) GUARDAR A ARCHIVOS ==============================================
timestamp = datestr(now,'yyyymmdd_HHMMSS');
xlsx_name = sprintf('resultados_lambda2_%s.xlsx', timestamp);
csv_name  = sprintf('resultados_lambda2_%s.csv',  timestamp);

writetable(results, xlsx_name);
writetable(results, csv_name);

fprintf('\nListo. Archivos guardados:\n  - %s\n  - %s\n', xlsx_name, csv_name);

%% === FUNCIONES LOCALES ===================================================
function [lam2_raw, lam2, isconn, remaining] = lambda2_after_removal(A, removed, tol)
    % Calcula lambda2 tras eliminar 'removed' (vector fila) en el grafo A
    n = size(A,1);
    keep = setdiff(1:n, removed);
    remaining = numel(keep);

    if remaining <= 1
        lam2_raw = NaN;  % no aplica
        lam2     = NaN;
        isconn   = false;
        return;
    end

    A_red = A(keep, keep);
    D_red = diag(sum(A_red,2));
    L_red = D_red - A_red;

    % Autovalores del laplaciano
    ev = sort(eig(L_red), 'ascend');  % reales para grafos no dirigidos
    lam2_raw = ev(2);

    % Recorte contra negativos diminutos por redondeo (numerics)
    lam2   = max(lam2_raw, 0);
    isconn = (lam2 > tol);
end
