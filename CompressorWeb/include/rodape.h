#ifndef RODAPE_H
#define RODAPE_H

// ============================================================================
// CSS PADRÃO DO RODAPÉ INDUSTRIAL EVIDENTE (PLAQUETA DE CRÉDITOS)
// Compatível com tema claro (RAL 7035) e temas escuros (.night-mode / .dark-mode)
// ============================================================================
#define ENERGIZAR_FOOTER_CSS                                                                              \
    "        .footer {\n"                                                                                 \
    "            width: 100%;\n"                                                                          \
    "            background: linear-gradient(180deg, #1e272e 0%, #141b20 100%);\n"                        \
    "            border-top: 4px solid #e65100;\n"                                                        \
    "            box-shadow: 0 -4px 14px rgba(0, 0, 0, 0.25), inset 0 1px 0 rgba(255, 255, 255, 0.08);\n" \
    "            color: #cfd8dc;\n"                                                                       \
    "            text-align: center;\n"                                                                   \
    "            padding: 14px 12px;\n"                                                                   \
    "            font-size: 12px;\n"                                                                      \
    "            line-height: 1.5;\n"                                                                     \
    "            margin-top: auto;\n"                                                                     \
    "        }\n"                                                                                         \
    "        .footer-brand {\n"                                                                           \
    "            font-size: 13px;\n"                                                                      \
    "            font-weight: 800;\n"                                                                     \
    "            color: #ff9800;\n"                                                                       \
    "            text-transform: uppercase;\n"                                                            \
    "            letter-spacing: 1px;\n"                                                                  \
    "            margin-bottom: 6px;\n"                                                                   \
    "        }\n"                                                                                         \
    "        .footer-credits {\n"                                                                         \
    "            max-width: 780px;\n"                                                                     \
    "            margin: 0 auto;\n"                                                                       \
    "            padding: 8px 14px;\n"                                                                    \
    "            background-color: rgba(0, 0, 0, 0.28);\n"                                                \
    "            border: 1px solid #37474f;\n"                                                            \
    "            border-left: 4px solid #ff9800;\n"                                                       \
    "            border-right: 4px solid #ff9800;\n"                                                      \
    "            border-radius: 5px;\n"                                                                   \
    "        }\n"                                                                                         \
    "        .footer-mentors {\n"                                                                         \
    "            font-size: 12px;\n"                                                                      \
    "            font-weight: 700;\n"                                                                     \
    "            color: #ffffff;\n"                                                                       \
    "            margin-bottom: 4px;\n"                                                                   \
    "        }\n"                                                                                         \
    "        .footer-mentors span {\n"                                                                    \
    "            color: #ffa726;\n"                                                                       \
    "            font-weight: 800;\n"                                                                     \
    "        }\n"                                                                                         \
    "        .footer-team {\n"                                                                            \
    "            font-size: 11.5px;\n"                                                                    \
    "            color: #eceff1;\n"                                                                       \
    "            font-weight: 600;\n"                                                                     \
    "        }\n"                                                                                         \
    "        .footer-team strong {\n"                                                                     \
    "            color: #80cbc4;\n"                                                                       \
    "            text-transform: uppercase;\n"                                                            \
    "            font-size: 11px;\n"                                                                      \
    "            letter-spacing: 0.4px;\n"                                                                \
    "        }\n"                                                                                         \
    "        .footer-inst {\n"                                                                            \
    "            margin-top: 6px;\n"                                                                      \
    "            font-size: 11px;\n"                                                                      \
    "            color: #90a4ae;\n"                                                                       \
    "            letter-spacing: 0.3px;\n"                                                                \
    "        }\n"                                                                                         \
    "        .footer-inst a {\n"                                                                          \
    "            display: inline;\n"                                                                      \
    "            background: transparent;\n"                                                              \
    "            border: none;\n"                                                                         \
    "            padding: 0;\n"                                                                           \
    "            text-transform: none;\n"                                                                 \
    "            font-size: 11px;\n"                                                                      \
    "            color: #ffb74d;\n"                                                                       \
    "            text-decoration: none;\n"                                                                \
    "            font-weight: 700;\n"                                                                     \
    "            margin-left: 6px;\n"                                                                     \
    "        }\n"                                                                                         \
    "        .footer-inst a:hover {\n"                                                                    \
    "            text-decoration: underline;\n"                                                           \
    "        }\n"                                                                                         \
    "        body.night-mode .footer,\n"                                                                  \
    "        body.dark-mode .footer {\n"                                                                  \
    "            background: linear-gradient(180deg, #0d1117 0%, #090c10 100%);\n"                        \
    "            border-top-color: #e65100;\n"                                                            \
    "        }\n"                                                                                         \
    "        body.night-mode .footer-credits,\n"                                                          \
    "        body.dark-mode .footer-credits {\n"                                                          \
    "            background-color: rgba(255, 255, 255, 0.03);\n"                                          \
    "            border-color: #30363d;\n"                                                                \
    "            border-left-color: #ff9800;\n"                                                           \
    "            border-right-color: #ff9800;\n"                                                          \
    "        }\n"

// ============================================================================
// HTML PADRÃO DO RODAPÉ INDUSTRIAL EVIDENTE
// ============================================================================
#define ENERGIZAR_FOOTER_HTML                                                                                                                              \
    "    <div class=\"footer\" role=\"contentinfo\">\n"                                                                                                    \
    "        <div class=\"footer-brand\">EnergizAR &bull; Automação Industrial e Monitoramento IoT</div>\n"                                                \
    "        <div class=\"footer-credits\">\n"                                                                                                             \
    "            <div class=\"footer-mentors\">\n"                                                                                                         \
    "                Instrutor / Arquiteto: <span>Lenon Yuri</span> &nbsp;|&nbsp; Co-autor: <span>Franco M. A. Caixeta</span>\n"                           \
    "            </div>\n"                                                                                                                                 \
    "            <div class=\"footer-team\">\n"                                                                                                            \
    "                <strong>Equipe Desenvolvedora:</strong> Brenda Luiza &bull; Davi Afonso &bull; João Martins &bull; Pedro Lucas &bull; Sofia Tresse\n" \
    "            </div>\n"                                                                                                                                 \
    "        </div>\n"                                                                                                                                     \
    "        <div class=\"footer-inst\">\n"                                                                                                                \
    "            Turma de Informática para Internet &bull; Programa Trilhas de Futuro (MG)\n"                                                              \
    "            <a href=\"/creditos\">[Ver Créditos]</a>\n"                                                                                               \
    "        </div>\n"                                                                                                                                     \
    "    </div>\n"

#endif