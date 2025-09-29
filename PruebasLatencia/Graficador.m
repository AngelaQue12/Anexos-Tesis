% Graficador.m
% Traza tipo Logic Analyzer desde CSV de Saleae

close all; clear; clc;

% === Configuración ===
csvPath = 'Nodo5-Nodo1-RedCompleta.csv';   % ruta al CSV
sampleTimeUnits = 's';                      % solo para la etiqueta del eje X
channelBaseOffset = 1.5;                    % separación vertical entre canales
lineWidth = 1.4;                            % grosor de línea

% === Cargar CSV (preservar nombres originales de columnas) ===
T = readtable(csvPath, 'VariableNamingRule', 'preserve');

% --- Tiempo ---
if any(strcmp(T.Properties.VariableNames, "Time [s]"))
    t = T.("Time [s]");
else
    % Fallback si el nombre difiere
    timeIdx = find(contains(T.Properties.VariableNames, "Time", 'IgnoreCase', true), 1, 'first');
    t = T{:, timeIdx};
end

% --- Columnas de canales (todas las que empiecen con 'Channel') ---
allNames = T.Properties.VariableNames;
chCols   = find(startsWith(allNames, "Channel"));
numCh    = numel(chCols);

% Matriz 0/1 de canales
X = zeros(height(T), numCh);
for k = 1:numCh
    X(:,k) = double(T{:, chCols(k)} > 0.5);  % forzar 0/1
end

% === Graficar ===
figure('Color','w'); hold on; grid on;

% Trazar cada canal con offset vertical.
% Usamos (numCh - k) para que Channel 0 quede arriba.
for k = 1:numCh
    y = X(:,k) + (numCh - k) * channelBaseOffset;
    stairs(t, y, 'LineWidth', lineWidth);
end

% --- Eje Y: ticks ASCENDENTES (requisito de MATLAB) ---
% Posiciones 0, 1.5, 3.0, ... hasta (numCh-1)*offset
yticks(0:channelBaseOffset:(numCh-1)*channelBaseOffset);

% Etiquetas invertidas para que arriba sea Channel 0
labels = arrayfun(@(idx) sprintf('Nodo %d', idx), 1:numCh, 'UniformOutput', false);
labels = fliplr(labels);   % invertir orden para que coincida con la disposición visual
yticklabels(labels);

xlabel(sprintf('Time [%s]', sampleTimeUnits));
ylabel('Digital Channels');
title('Recorrido de N5-N1 y N1-N5 Falta Nodo 3');
% xlim([min(t) max(t)]);
xlim([2.5 5.5]);  % muestra solo ese rango
ylim([-0.5, (numCh-1)*channelBaseOffset + 1]);  % un poco de margen

% === (Opcional) marcar transiciones ===
% for k = 1:numCh
%     y  = X(:,k) + (numCh - k) * channelBaseOffset;
%     dy = [0; diff(y)];
%     trIdx = find(dy ~= 0);
%     plot(t(trIdx), y(trIdx), 'o', 'MarkerSize', 3);
% end

% === (Opcional) exportar PNG ===
% print('-dpng','-r200','logic_plot.png');
